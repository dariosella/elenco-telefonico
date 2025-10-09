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

#include "helper.h"
#include "user.h"
#include "contact.h"

// SOCKET DI ASCOLTO
int l_sock = -1;

// MUTEX
pthread_mutex_t r_mutex; // protezione scrittura su rubrica
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
void handleSendReturn(ssize_t ret, int *sock);
void handleRecvReturn(ssize_t ret, int *sock);

int main(int argc, char *argv[]){
	struct sockaddr_in server;
	struct sockaddr_in client;
	// SOCKET DI CONNESSIONE
	int c_sock = -1;
	
	// INIZIALIZZAZIONE MUTEX
	pthread_mutex_init(&r_mutex, NULL);
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
	socklen_t c_size;
	while (1){
		c_size = sizeof(client);
		if ((c_sock = accept(l_sock, (struct sockaddr *)&client, &c_size)) < 0){
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
		if (setsockopt(c_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
			perror("setsockopt SO_RCVTIMEO");
			close(c_sock);
			continue;
		}
		
		// TIMEOUT PER L'INVIO
		if (setsockopt(c_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0){
			perror("setsockopt SO_SNDTIMEO");
			close(c_sock);
			continue;
		}
		
		int *c_sockptr = malloc(sizeof(int));
		if (c_sockptr == NULL){
			perror("malloc c_sockptr");
			continue;
		}
		*c_sockptr = c_sock; // copia separata da passare al thread
		
		if (pthread_create(&tid, NULL, clientThread, (void *)c_sockptr) < 0){
			perror("pthread_create");
			close(c_sock);
			free(c_sockptr);
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
	
	pthread_cleanup_push((void(*)(void*))free,  c_user);
	pthread_cleanup_push((void(*)(void*))close, (void*)(intptr_t)c_sock);
	
	// SCELTA DEL CLIENT
	while (res != 0){
		handleRecvReturn(safeRecv(c_sock, &net_choice, sizeof(net_choice), 0), &c_sock);
		choice = ntohl(net_choice);
		switch (choice){
			case 1:
				// REGISTRAZIONE
				// RICEVO USERNAME, PASSWORD, PERMESSO DAL CLIENT
				handleRecvReturn(safeRecv(c_sock, c_user->usr, USR_SIZE, 0), &c_sock);
				handleRecvReturn(safeRecv(c_sock, c_user->pwd, PWD_SIZE, 0), &c_sock);
				handleRecvReturn(safeRecv(c_sock, perm, PERM_SIZE, 0), &c_sock);
				
				// SEZIONE CRITICA
				pthread_mutex_lock(&u_mutex);
				res = usrRegister(c_user, perm);
				pthread_mutex_unlock(&u_mutex);
				
				net_res = htonl(res);
				handleSendReturn(safeSend(c_sock, &net_res, sizeof(net_res), 0), &c_sock);
				break;
			case 2:
				// LOGIN
				// RICEVO USERNAME E PASSWORD DAL CLIENT
				handleRecvReturn(safeRecv(c_sock, c_user->usr, USR_SIZE, 0), &c_sock);
				handleRecvReturn(safeRecv(c_sock, c_user->pwd, PWD_SIZE, 0), &c_sock);
				
				res = usrLogin(c_user);
				
				net_res = htonl(res);
				handleSendReturn(safeSend(c_sock, &net_res, sizeof(net_res), 0), &c_sock);
				break;
			default:
				break;
		}
	}
	
	int answer;
	uint8_t net_answer;
	do {
		// SCELTA DEL CLIENT
		handleRecvReturn(safeRecv(c_sock, &net_choice, sizeof(net_choice), 0), &c_sock);
		choice = ntohl(net_choice);
		switch (choice){
			case 1:
				// AGGIUNGI CONTATTO
				answer = checkPermission(c_user->usr, "w");
				if (answer == -1){
					pthread_exit(NULL);
				}
				
				net_answer = answer;
				handleSendReturn(safeSend(c_sock, &net_answer, sizeof(net_answer), 0), &c_sock);
				if (answer){
					// CLIENT AUTORIZZATO
					char buffer[BUF_SIZE];
					
					handleRecvReturn(safeRecv(c_sock, buffer, BUF_SIZE, 0), &c_sock);
					Contact *contatto = createContact(buffer);
					if (contatto == NULL){
						perror("malloc contact");
						pthread_exit(NULL);
					}
					
					memset(buffer, 0, BUF_SIZE);
					
					// SEZIONE CRITICA
					pthread_mutex_lock(&r_mutex);
					res = addContact(contatto, buffer);
					pthread_mutex_unlock(&r_mutex);
					
					if (res == 0){
						handleSendReturn(safeSend(c_sock, buffer, BUF_SIZE, 0), &c_sock);
					} else {
						strcpy(buffer, "Errore\n");
						handleSendReturn(safeSend(c_sock, buffer, BUF_SIZE, 0), &c_sock);
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
				handleSendReturn(safeSend(c_sock, &net_answer, sizeof(net_answer), 0), &c_sock);
				if (answer){
					// CLIENT AUTORIZZATO
					char buffer[BUF_SIZE];
					
					handleRecvReturn(safeRecv(c_sock, buffer, BUF_SIZE, 0), &c_sock);
					Contact *contatto = createContact(buffer);
					if (contatto == NULL){
						perror("malloc contatto");
						pthread_exit(NULL);
					}
					
					memset(buffer, 0, BUF_SIZE);
					res = searchContact(contatto, buffer);
					
					if (res == 0){
						handleSendReturn(safeSend(c_sock, buffer, BUF_SIZE, 0), &c_sock);
					} else {
						strcpy(buffer, "Errore\n");
						handleSendReturn(safeSend(c_sock, buffer, BUF_SIZE, 0), &c_sock);
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
	
	// SIGINT
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = interruptHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1){
		perror("sigaction SIGINT");
		exit(EXIT_FAILURE);
	}
	
	// SIGTERM
	if (sigaction(SIGTERM, &sa, NULL) == -1){
		perror("sigaction SIGTERM");
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
	
	// SIGHUP
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
	
	if (*sPort == NULL){
		puts("porta mancante");
		exit(EXIT_FAILURE);
	}
	
	return 0;
}

void handleSendReturn(ssize_t ret, int *sock){
	if (ret == -3) {
		puts("Connessione chiusa dal client");
		close(*sock);
		pthread_exit(NULL);
	} else if (ret == -2){
		puts("Tempo scaduto");
		close(*sock);
		pthread_exit(NULL);
	} else if (ret == -1) {
		perror("send");
		close(*sock);
		pthread_exit(NULL);
	}
}

void handleRecvReturn(ssize_t ret, int *sock){
    if (ret == -3 || ret == 0) {
      puts("Connessione chiusa dal client");
      close(*sock);
      pthread_exit(NULL);
    } else if (ret == -2){
    	puts("Tempo scaduto");
    	close(*sock);
    	pthread_exit(NULL);
    }
    else if (ret == -1) {
    	perror("recv");
 	    close(*sock);
      pthread_exit(NULL);
    }
}

