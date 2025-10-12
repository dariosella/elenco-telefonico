#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <semaphore.h>

#include "helper.h"
#include "user.h"
#include "contact.h"

// SOCKET DI ASCOLTO
int l_sock = -1;

typedef struct {
	sem_t turnstile; // 1 aperto, 0 chiuso (c'è un writer in attesa)
	sem_t roomEmpty; // 1 vuoto, 0 occupato
	sem_t mutex;
	int readers;
} rwsem_t;

rwsem_t rw;

pthread_mutex_t u_mutex; // protezione scrittura su utenti

// SEGNALI
void interruptHandler(int sig){
	if (l_sock >= 0)
		close(l_sock);
	_exit(EXIT_SUCCESS);
}

void signalSetup();
void *clientThread(void *arg);
int parseCmdLine(int argc, char *argv[], char **sPort);
void handleSendReturn(ssize_t ret);
void handleRecvReturn(ssize_t ret);

int main(int argc, char *argv[]){
	struct sockaddr_in server;
	struct sockaddr_in client;
	// SOCKET DI CONNESSIONE
	int conn_sock = -1;
	
	// INIZIALIZZAZIONE SEMAFORI
	if (sem_init(&rw.turnstile, 0, 1) == -1) {
		perror("sem_init turnstile");
		exit(EXIT_FAILURE);
	}
	if (sem_init(&rw.roomEmpty, 0, 1) == -1) {
		perror("sem_init roomEmpty");
		exit(EXIT_FAILURE);
	}
	if (sem_init(&rw.mutex, 0, 1) == -1) {
		perror("sem_init mutex");
		exit(EXIT_FAILURE);
	}
	rw.readers = 0;
	pthread_mutex_init(&u_mutex, NULL);
	
	// SEGNALI
	signalSetup();
	
	char *sPort = NULL, *end = NULL;
	parseCmdLine(argc, argv, &sPort);
	
	long lport = strtol(sPort, &end, 10);
	if (*end != '\0' || lport < 1 || lport > 65535){
		puts("porta non riconosciuta");
		exit(EXIT_FAILURE);
	}
	uint16_t port = (uint16_t)lport;
	
	printf("Server in ascolto sulla porta %u\n", port);
	if ( (l_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	// riuso della porta per restart veloci
	int opt = 1;
	if (setsockopt(l_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
		perror("setsockopt SO_REUSEADDR");
		exit(EXIT_FAILURE);
	}
	
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(l_sock, (struct sockaddr *)&server, sizeof(server)) < 0){
		perror("bind");
		exit(EXIT_FAILURE);
	}
	
	if (listen(l_sock, LISTENQ) < 0){
		perror("listen");
		exit(EXIT_FAILURE);
	}
	
	pthread_t tid;
	socklen_t conn_size;
	while (1){
		conn_size = sizeof(client);
		if ((conn_sock = accept(l_sock, (struct sockaddr *)&client, &conn_size)) < 0){
			if (errno == EINTR)
				continue; // riprova
			perror("accept");
			exit(EXIT_FAILURE);
		}
		
		printf("Connessione al server da %s\n", inet_ntoa(client.sin_addr));
		
		struct timeval tv;
		tv.tv_sec = TIMEOUT;
		tv.tv_usec = 0;
		
		// TIMEOUT PER LA RICEZIONE
		if (setsockopt(conn_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
			perror("setsockopt SO_RCVTIMEO");
			close(conn_sock);
			continue;
		}
		
		// TIMEOUT PER L'INVIO
		if (setsockopt(conn_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0){
			perror("setsockopt SO_SNDTIMEO");
			close(conn_sock);
			continue;
		}
		
		int *conn_sockptr = malloc(sizeof(int));
		if (conn_sockptr == NULL){
			perror("malloc c_sockptr");
			continue;
		}
		*conn_sockptr = conn_sock; // copia separata da passare al thread
		
		if (pthread_create(&tid, NULL, clientThread, (void *)conn_sockptr) < 0){
			perror("pthread_create");
			close(conn_sock);
			free(conn_sockptr);
			continue;
		}
		
		pthread_detach(tid); // se il thread termina non devo joinarlo per rilasciare le risorse
	}
	
	return 0;
}

void *clientThread(void *arg){
	int c_sock = *((int *)arg);
	free(arg);
	
	int res = -1, choice;
	uint32_t net_res, net_choice;
	
	// creazione utente del client
	char perm[PERM_SIZE];
	User *c_user = malloc(sizeof(User));
	if (c_user == NULL){
		perror("malloc");
		close(c_sock);
		pthread_exit(NULL);
	}
	c_user->usr[0] = '\0';
	c_user->pwd[0] = '\0';
	c_user->prm[0] = '\0';
	
	pthread_cleanup_push((void(*)(void*))free,  c_user);
	pthread_cleanup_push((void(*)(void*))close, (void*)(intptr_t)c_sock);
	
	// SCELTA DEL CLIENT
	while (res != 0){
		handleRecvReturn(safeRecv(c_sock, &net_choice, sizeof(net_choice), 0));
		choice = ntohl(net_choice);
		switch (choice){
			case 1:
				// REGISTRAZIONE
				// RICEVO USERNAME, PASSWORD, PERMESSO DAL CLIENT
				handleRecvReturn(safeRecv(c_sock, c_user->usr, USR_SIZE, 0));
				handleRecvReturn(safeRecv(c_sock, c_user->pwd, PWD_SIZE, 0));
				handleRecvReturn(safeRecv(c_sock, c_user->prm, PERM_SIZE, 0));
				
				// SEZIONE CRITICA
				pthread_mutex_lock(&u_mutex);
				res = usrRegister(c_user);
				pthread_mutex_unlock(&u_mutex);
				
				net_res = htonl(res);
				handleSendReturn(safeSend(c_sock, &net_res, sizeof(net_res), 0));
				break;
			case 2:
				// LOGIN
				// RICEVO USERNAME E PASSWORD DAL CLIENT
				handleRecvReturn(safeRecv(c_sock, c_user->usr, USR_SIZE, 0));
				handleRecvReturn(safeRecv(c_sock, c_user->pwd, PWD_SIZE, 0));
				
				res = usrLogin(c_user);
				
				net_res = htonl(res);
				handleSendReturn(safeSend(c_sock, &net_res, sizeof(net_res), 0));
				break;
			default:
				break;
		}
	}
	
	int answer;
	uint8_t net_answer;
	do {
		// SCELTA DEL CLIENT
		handleRecvReturn(safeRecv(c_sock, &net_choice, sizeof(net_choice), 0));
		choice = ntohl(net_choice);
		switch (choice){
			case 1:
				// AGGIUNGI CONTATTO
				answer = checkPermission(c_user->usr, "w");
				if (answer == -1){
					pthread_exit(NULL);
				}
				
				net_answer = answer;
				handleSendReturn(safeSend(c_sock, &net_answer, sizeof(net_answer), 0));
				if (answer){
					// CLIENT AUTORIZZATO
					char buffer[BUF_SIZE];
					
					handleRecvReturn(safeRecv(c_sock, buffer, BUF_SIZE, 0));
					Contact *contatto = createContact(buffer);
					if (contatto == NULL){
						perror("malloc contact");
						pthread_exit(NULL);
					}
					
					memset(buffer, 0, BUF_SIZE);
					
					// WRITER
					sem_wait(&rw.turnstile); // impedisci a nuovi lettori di entrare
					sem_wait(&rw.roomEmpty); // aspetta che la stanza sia vuota
					
					res = addContact(contatto, buffer);
					
					sem_post(&rw.roomEmpty); // libera la stanza
					sem_post(&rw.turnstile); // riapri il tornello
					
					if (res == 0){
						handleSendReturn(safeSend(c_sock, buffer, BUF_SIZE, 0));
					} else {
						strcpy(buffer, "Errore\n");
						handleSendReturn(safeSend(c_sock, buffer, BUF_SIZE, 0));
					}
					
					free(contatto);
				}
				break;
			case 2:
				// CERCA CONTATTO
				answer = checkPermission(c_user->usr, "r");
				if (answer == -1){
					pthread_exit(NULL);
				}
				
				net_answer = answer;
				handleSendReturn(safeSend(c_sock, &net_answer, sizeof(net_answer), 0));
				if (answer){
					// CLIENT AUTORIZZATO
					char buffer[BUF_SIZE];
					
					handleRecvReturn(safeRecv(c_sock, buffer, BUF_SIZE, 0));
					Contact *contatto = createContact(buffer);
					if (contatto == NULL){
						perror("malloc contatto");
						pthread_exit(NULL);
					}
					
					memset(buffer, 0, BUF_SIZE);
					
					// READER ENTER
					sem_wait(&rw.turnstile);   // se c'è uno scrittore in attesa, aspetta
					sem_post(&rw.turnstile);   // passa subito se nessuno scrittore ha chiuso il tornello
					sem_wait(&rw.mutex);
					rw.readers++;
					if (rw.readers == 1){
						// primo lettore blocca la stanza agli scrittori
						sem_wait(&rw.roomEmpty);
					}
					sem_post(&rw.mutex);
					
					// LETTURA
					res = searchContact(contatto, buffer);
					
					// READER EXIT
					sem_wait(&rw.mutex);
					rw.readers--;
					if (rw.readers == 0){
						// ultimo lettore libera la stanza
						sem_post(&rw.roomEmpty);
					}
					sem_post(&rw.mutex);
					
					if (res == 0){
						handleSendReturn(safeSend(c_sock, buffer, BUF_SIZE, 0));
					} else {
						strcpy(buffer, "Errore\n");
						handleSendReturn(safeSend(c_sock, buffer, BUF_SIZE, 0));
					}
					
					free(contatto);
				}
				break;
			case 3:
				break;
			default:
				break;
		}
	} while (choice != 3);
	
	pthread_cleanup_pop(1); // close(c_sock)
	pthread_cleanup_pop(1); // free(c_user)
	return NULL;
}

void signalSetup(){
	struct sigaction sa;
	
	// SIGINT, SIGTERM
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = interruptHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1){
		perror("sigaction SIGINT");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGTERM, &sa, NULL) == -1){
		perror("sigaction SIGTERM");
		exit(EXIT_FAILURE);
	}
	
	// SIGPIPE, SIGHUP
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGPIPE, &sa, NULL) == -1){
		perror("sigaction SIGPIPE");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGHUP, &sa, NULL) == -1){
		perror("sigaction SIGHUP");
		exit(EXIT_FAILURE);
	}
	
}

int parseCmdLine(int argc, char *argv[], char **sPort) {
	if (argc < 3){
		printf("Usage: %s -p (port) [-h]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	for (int i = 1; i < argc; i++){
		if ((!strcmp(argv[i], "-p") || !strcmp(argv[i], "-P")) && i + 1 < argc){
			*sPort = argv[i + 1];
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-H")){
			printf("Usage: %s -p (port) [-h]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	
  if (!*sPort){
      puts("Errore: -p è obbligatorio");
      exit(EXIT_FAILURE);
  }
	
	return 0;
}

void handleSendReturn(ssize_t ret){
	if (ret == -3) {
		puts("Connessione chiusa dal client");
		pthread_exit(NULL);
	} else if (ret == -2){
		puts("Tempo scaduto");
		pthread_exit(NULL);
	} else if (ret == -1) {
		perror("send");
		pthread_exit(NULL);
	}
}

void handleRecvReturn(ssize_t ret){
    if (ret == -3) {
      puts("Connessione chiusa dal client");
      pthread_exit(NULL);
    } else if (ret == -2){
    	puts("Tempo scaduto");
    	pthread_exit(NULL);
    }
    else if (ret == -1) {
    	perror("recv");
      pthread_exit(NULL);
    }
}

