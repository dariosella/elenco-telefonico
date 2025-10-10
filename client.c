#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>

#include "helper.h"

// SOCKET DI CONNESSIONE
int c_sock = -1;

// SIGALRM
volatile sig_atomic_t timeout_expired = 0;
void alarmHandler(int sig){
	timeout_expired = 1;
}

// SIGINT
void interruptHandler(int sig){
	if (c_sock >= 0)
		close(c_sock);
	_exit(EXIT_SUCCESS);
}

// FUNZIONI
int parseCmdLine(int argc, char *argv[], char **sAddr, char **sPort);
void signalSetup();
ssize_t safeInputAlarm(int fd, void *buffer, size_t size);
void handleInputReturn(ssize_t ret);
void handleSendReturn(ssize_t ret);
void handleRecvReturn(ssize_t ret);
void handleNewline(char *buf);
void flushInput(unsigned int seconds);

int main(int argc, char *argv[]){
	struct sockaddr_in server;
	struct hostent *hp;
	
	signalSetup();
	
	char *sAddr = NULL, *sPort = NULL, *end = NULL;
	parseCmdLine(argc, argv, &sAddr, &sPort);
	
	long lport = strtol(sPort, &end, 10);
	if (*end != '\0' || lport < 1 || lport > 65535){
		puts("porta non riconosciuta");
		exit(EXIT_FAILURE);
	}
	
	uint16_t port = lport;
	
	if ( (c_sock = socket(AF_INET, SOCK_STREAM, 0) ) < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	if (inet_aton(sAddr, &server.sin_addr) <= 0){
		if ( (hp = gethostbyname(sAddr)) == NULL){
			herror("gethostbyname");
			exit(EXIT_FAILURE);
		}
		server.sin_addr = *((struct in_addr *)hp->h_addr);
	}
	
	if (connect(c_sock, (struct sockaddr *)&server, sizeof(server)) < 0){
		perror("connect");
		exit(EXIT_FAILURE);
	}
	
	struct timeval tv;
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	
  // TIMEOUT PER LA RICEZIONE
  if (setsockopt(c_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
      perror("setsockopt SO_RCVTIMEO");
      close(c_sock);
      exit(EXIT_FAILURE);
  }
  
  // TIMEOUT PER L'INVIO
  if (setsockopt(c_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
      perror("setsockopt SO_SNDTIMEO");
      close(c_sock);
      exit(EXIT_FAILURE);
  }
	
	char username[USR_SIZE];
	char password[PWD_SIZE];
	char perm[PERM_SIZE];
	int res = -1;
	uint32_t net_res;
	
	// SCELTA DELL'UTENTE PER LOGIN O REGISTRAZIONE
	char choice_str[CHOICE_SIZE];
	int choice;
	uint32_t net_choice;
	while (res != 0){
		// INPUT SCELTA
		printf("%s\n%s\n%s\n", "Scegliere se:", "1. Registrarti", "2. Loggarti");
		memset(choice_str, 0, CHOICE_SIZE);
		handleInputReturn(safeInputAlarm(0, choice_str, CHOICE_SIZE));
		handleNewline(choice_str);
		
		// CONVERSIONE SCELTA
		choice = strtol(choice_str, &end, 10);
		if (*end != '\0' || choice < 1 || choice > 2){
			puts("Input non valido");
			continue;
		}
		
		// INVIO SCELTA AL SERVER
		net_choice = htonl(choice);
		handleSendReturn(safeSend(c_sock, &net_choice, sizeof(net_choice), 0));
		switch (choice){
			case 1:
				// REGISTRAZIONE
				memset(username, 0, USR_SIZE);
				memset(password, 0, PWD_SIZE);
				memset(perm, 0, PERM_SIZE);
				
				// USERNAME INPUT
				printf("%s", "Inserisci username: ");
				fflush(stdout);
				handleInputReturn(safeInputAlarm(0, username, USR_SIZE));
				handleNewline(username);
				
				// PASSWORD INPUT
				printf("%s", "Inserisci password: ");
				fflush(stdout);
				handleInputReturn(safeInputAlarm(0, password, PWD_SIZE));
				handleNewline(password);
				
				// PERMESSO INPUT
				printf("%s\n%s\n%s\n%s\n", "Inserisci un permesso tra:", "lettura -> 'r'", "scrittura -> 'w'", "lettura e scrittura -> 'rw'");
				fflush(stdout);
				handleInputReturn(safeInputAlarm(0, perm, PERM_SIZE));
				handleNewline(perm);
				
				// PERMESSO CHECK
				while (strcmp(perm, "r") != 0 && strcmp(perm, "w") != 0 && strcmp(perm, "rw") != 0){
					memset(perm, 0, PERM_SIZE);
					printf("%s\n%s\n%s\n%s\n", "Inserisci un permesso tra:", "lettura -> 'r'", "scrittura -> 'w'", "lettura e scrittura -> 'rw'");
					handleInputReturn(safeInputAlarm(0, perm, PERM_SIZE));
					handleNewline(perm);
				}
				
				// INVIO USERNAME, PASSWORD, PERMESSO AL SERVER
				handleSendReturn(safeSend(c_sock, username, USR_SIZE, 0));
				handleSendReturn(safeSend(c_sock, password, PWD_SIZE, 0));
				handleSendReturn(safeSend(c_sock, perm, PERM_SIZE, 0));
				
				// RISPOSTA DEL SERVER
			  handleRecvReturn(safeRecv(c_sock, &net_res, sizeof(net_res), 0));
				res = ntohl(net_res);
				switch (res){
					case 1:
						printf("%s già utilizzato\n", username);
						fflush(stdout);
						break;
					case -1:
						puts("Errore");
						break;
				}
				break;
			case 2:
				// LOGIN
				memset(username, 0, USR_SIZE);
				memset(password, 0, PWD_SIZE);
				
				// USERNAME INPUT
				printf("%s", "Inserisci username: ");
				fflush(stdout);
				handleInputReturn(safeInputAlarm(0, username, USR_SIZE));
				handleNewline(username);
				
				// PASSWORD INPUT
				printf("%s", "Inserisci password: ");
				fflush(stdout);
				handleInputReturn(safeInputAlarm(0, password, PWD_SIZE));
				handleNewline(password);
				
				// INVIO USERNAME, PASSWORD AL SERVER
				handleSendReturn(safeSend(c_sock, username, USR_SIZE, 0));
				handleSendReturn(safeSend(c_sock, password, PWD_SIZE, 0));
				
				// RISPOSTA DAL SERVER
				handleRecvReturn(safeRecv(c_sock, &net_res, sizeof(net_res), 0));
				res = ntohl(net_res);
				switch (res){
					case 1:
						puts("Password sbagliata");
						break;
					case 2:
						printf("%s Non esiste\n", username);
						fflush(stdout);
						break;
					case -1:
						puts("Errore");
						break;
				}
				break;
			default:
				puts("Scelta non valida");
		}
	}
	
	// LOGIN/REGISTRAZIONE RIUSCITO
	printf("Benvenuto %s!\n", username);
	fflush(stdout);
	
	uint8_t answer_u8;
	bool answer;
	do {
		// INPUT SCELTA
		printf("%s\n%s\n%s\n%s\n", "Scegliere:", "1. Aggiungere contatto", "2. Cercare contatto", "3. Uscire");
		fflush(stdout);
		memset(choice_str, 0, CHOICE_SIZE);
		handleInputReturn(safeInputAlarm(0, choice_str, CHOICE_SIZE));
		handleNewline(choice_str);
		
		// CONVERSIONE SCELTA
		choice = strtol(choice_str, &end, 10);
		if (*end != '\0' || choice < 1 || choice > 3){
			puts("Input non valido");
			continue;
		}
		
		// INVIO SCELTA AL SERVER
		net_choice = htonl(choice);
		handleSendReturn(safeSend(c_sock, &net_choice, sizeof(net_choice), 0));
		
		switch (choice){
			case 1:
				// AGGIUNGI CONTATTO
				// RICEVO AUTORIZZAZIONE DAL SERVER
				handleRecvReturn(safeRecv(c_sock, &answer_u8, sizeof(answer_u8), 0));
				answer = (answer_u8 != 0);
				
				if (answer){
					// CLIENT AUTORIZZATO
					char contatto[BUF_SIZE];
					memset(contatto, 0, BUF_SIZE);
					
					// INPUT CONTATTO
					printf("%s\n", "Inserisci 'Nome [Nomi secondari] Cognome Numero'");
					fflush(stdout);
					handleInputReturn(safeInputAlarm(0, contatto, BUF_SIZE));
					handleNewline(contatto);
					
					// INVIO CONTATTO AL SERVER
					handleSendReturn(safeSend(c_sock, contatto, BUF_SIZE, 0));
					
					char risposta[BUF_SIZE];
					memset(risposta, 0, BUF_SIZE);
					
					// RISPOSTA DAL SERVER
					handleRecvReturn(safeRecv(c_sock, risposta, BUF_SIZE, 0));
					printf("%s\n", risposta);
					fflush(stdout);
				} else {
					// CLIENT NON AUTORIZZATO
					puts("Non hai i permessi necessari per aggiungere contatti");
				}
				break;
			case 2:
				// CERCA CONTATTO
				// RICEVO AUTORIZZAZIONE DAL SERVER
				handleRecvReturn(safeRecv(c_sock, &answer_u8, sizeof(answer_u8), 0));
				answer = (answer_u8 != 0);
				
				if (answer){
					// CLIENT AUTORIZZATO
					char nome[BUF_SIZE];
					memset(nome, 0, BUF_SIZE);
					
					// INPUT CONTATTO
					printf("%s\n", "Inserisci 'Nome [Nomi secondari] Cognome'");
					fflush(stdout);
					handleInputReturn(safeInputAlarm(0, nome, BUF_SIZE));
					handleNewline(nome);
					
					// INVIO CONTATTO AL SERVER
					handleSendReturn(safeSend(c_sock, nome, BUF_SIZE, 0));
					
					char risposta[BUF_SIZE];
					memset(risposta, 0, BUF_SIZE);
					
					// RISPOSTA DAL SERVER
					handleRecvReturn(safeRecv(c_sock, risposta, BUF_SIZE, 0));
					printf("%s\n", risposta);
					fflush(stdout);
				} else {
					// CLIENT NON AUTORIZZATO
					puts("Non hai i permessi necessari per cercare contatti");
				}
				break;
			case 3:
				// USCITA
				// CHIUSURA SOCKET DI CONNESSIONE
				close(c_sock);
				break;
			default:
				puts("Scelta non valida");
		}
	} while (choice != 3);
	
	exit(EXIT_SUCCESS);
}

void signalSetup(){
	struct sigaction sa;
	
  // SIGALRM
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = alarmHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGALRM, &sa, NULL) == -1) { perror("sigaction SIGALRM"); exit(EXIT_FAILURE); }
	
  // SIGINT, SIGTERM, SIGHUP, SIGQUIT
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = interruptHandler;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGALRM); // blocca SIGALRM
  sa.sa_flags = 0;
  if (sigaction(SIGINT, &sa, NULL) == -1){ 
  	perror("sigaction SIGINT"); 
  	exit(EXIT_FAILURE); 
  }
	if (sigaction(SIGTERM, &sa, NULL) == -1){ 
		perror("sigaction SIGTERM");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGHUP, &sa, NULL) == -1){
		perror("sigaction SIGHUP");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGQUIT, &sa, NULL) == -1){
		perror("sigaction SIGQUIT");
		exit(EXIT_FAILURE);
	}
		
  // SIGPIPE
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGPIPE, &sa, NULL) == -1){
		perror("sigaction SIGPIPE");
		exit(EXIT_FAILURE);
	}
}

int parseCmdLine(int argc, char *argv[], char **sAddr, char **sPort){
	if (argc < 5){
		printf("Utilizzo: %s -a (indirizzo remoto) -p (porta remota) [-h]\n", argv[0]);
		fflush(stdout);
		exit(EXIT_FAILURE);
	}
	
	for (int i = 1; i < argc; i++){
		if ((!strcmp(argv[i], "-a") || !strcmp(argv[i], "-A")) && i + 1 < argc){
			*sAddr = argv[i + 1];
		} else if ((!strcmp(argv[i], "-p") || !strcmp(argv[i], "-P")) && i + 1 < argc){
			*sPort = argv[i + 1];
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-H")){
			printf("Utilizzo: %s -a (indirizzo remoto) -p (porta remota) [-h]\n", argv[0]);
			fflush(stdout);
			exit(EXIT_SUCCESS);
		}
	}
	
   if (!*sAddr || !*sPort){
       puts("Errore: -a e -p sono obbligatori");
       exit(EXIT_FAILURE);
   }

	return 0;
}

ssize_t safeInputAlarm(int fd, void *buffer, size_t size){
    if (size == 0)
        return 0;

    char *ptr = buffer;
    size_t i = 0;
    ssize_t r;
    char c;

    timeout_expired = 0;
    alarm(TIMEOUT);

    while (i < size - 1) {
        r = read(fd, &c, 1);

        if (r == 1) {
            ptr[i++] = c;
            if (c == '\n')
                break; // fine riga
        } 
        else if (r == 0) {
            // EOF
            break;
        } 
        else if (r == -1) {
            if (errno == EINTR && !timeout_expired)
                continue; // riprova
            else {
                alarm(0);
                ptr[i] = '\0';
                return timeout_expired ? -2 : -1; // -2 timeout, -1 errore
            }
        }
    }

    alarm(0);
    ptr[i] = '\0';

    // ritorna numero di byte letti (escluso terminatore)
    return i;
}

void handleNewline(char *buf){
	size_t len = strlen(buf);
	if (len == 0) return;
	
	if (strchr(buf, '\n') != NULL){
		buf[strcspn(buf, "\n")] = '\0';
	} else {
		flushInput(2);
		puts("input troppo lungo");
		close(c_sock);
		exit(EXIT_FAILURE);
	}
}

void flushInput(unsigned int seconds){
	char c;
	ssize_t r;
	timeout_expired = 0;
	alarm(seconds);
  while (1) {
		r = read(0, &c, 1);
    if (r == 1) {
			if (c == '\n')
				break;
    }
    else if (r == 0) {
			break; // EOF
		} 
    else if (r == -1) {
    	if (errno == EINTR && !timeout_expired)
				continue;
      break; // errore o timeout
    }
	}
  alarm(0);
    
  if (timeout_expired){
  	puts("flush timeout");
   	close(c_sock);
   	exit(EXIT_FAILURE);
  } else if (r == -1){
  	perror("flush error");
   	close(c_sock);
   	exit(EXIT_FAILURE);
  }
}

void handleInputReturn(ssize_t ret){
	if (ret == -2){
		puts("Tempo scaduto");
		close(c_sock);
		exit(EXIT_FAILURE);
	} else if (ret == -1){
		perror("read");
		close(c_sock);
		exit(EXIT_FAILURE);
	}
}

void handleSendReturn(ssize_t ret){
	if (ret == -3) {
		puts("Connessione chiusa dal server");
		close(c_sock);
		exit(EXIT_FAILURE);
	} else if (ret == -2){
		puts("Tempo scaduto");
		close(c_sock);
		exit(EXIT_FAILURE);
	} else if (ret == -1) {
		perror("send");
		close(c_sock);
		exit(EXIT_FAILURE);
	}
}

void handleRecvReturn(ssize_t ret){
    if (ret == -3 || ret == 0) {
      puts("Connessione chiusa dal server");
      close(c_sock);
      exit(EXIT_FAILURE);
    } else if (ret == -2){
    	puts("Tempo scaduto");
    	close(c_sock);
    	exit(EXIT_FAILURE);
    }
    else if (ret == -1) {
    	perror("recv");
 	    close(c_sock);
      exit(EXIT_FAILURE);
    }
}

