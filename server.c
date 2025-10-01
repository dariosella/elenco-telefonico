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

#include "helper.h"
#include "user.h"
#include "contact.h"

#define LISTENQ (8)

pthread_mutex_t r_mutex;
pthread_mutex_t up_mutex;

void *clientThread(void *arg); // avvio thread

int parseCmdLine(int argc, char *argv[], char **sPort);
bool checkPermission(char *username, char *perm);

int main(int argc, char *argv[]){
	pthread_mutex_init(&r_mutex, NULL); // inizializza mutex per lettura/scrittura su rubrica
	pthread_mutex_init(&up_mutex, NULL); // mutex per login/registra utenti
	int l_sock, c_sock; // listen socket e client socket
	struct sockaddr_in server;
	struct sockaddr_in client;
	
	char *sPort; // porta su cui il server ascolta
	parseCmdLine(argc, argv, &sPort); // acquisizione porta passata come argomento da linea di comando
	
	char *end;
	int port = strtol(sPort, &end, 0);
	if (*end){
		// entra solo se *end non è \0
		puts("porta non riconosciuta");
		exit(EXIT_FAILURE);
	}
	
	printf("server in ascolto sulla porta %d\n", port);
	l_sock = socket(AF_INET, SOCK_STREAM, 0); // creazione listen socket
	if (l_sock < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	
	// associazione listen socket con indirizzo
	if (bind(l_sock, (struct sockaddr *)&server, sizeof(server)) < 0){
		perror("bind");
		exit(EXIT_FAILURE);
	}
	
	// server in ascolto
	if (listen(l_sock, LISTENQ) < 0){
		perror("listen");
		exit(EXIT_FAILURE);
	}
	
	pthread_t tid;
	socklen_t c_size = sizeof(client);
	while (1){
		// accetta la connessione del client
		if ((c_sock = accept(l_sock, (struct sockaddr *)&client, &c_size)) < 0){
			perror("accept");
			exit(EXIT_FAILURE);
		}
		
		printf("connessione al server da %s\n", inet_ntoa(client.sin_addr));
		
		int *c_sockptr = malloc(sizeof(int));
		if (c_sockptr == NULL){
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		
		*c_sockptr = c_sock; // copia separata da passare al thread
		
		if (pthread_create(&tid, NULL, clientThread, (void *)c_sockptr) < 0){
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
		
		pthread_detach(tid); // se il thread termina non devo joinarlo per rilasciare le risorse
	}
	
	return 0;
}

void *clientThread(void *arg){
	int c_sock = *((int *)arg);
	free(arg); // evita memory leak
	int res = -1, net_res = -1;
	int choice, net_choice;
	bool answer;
	
	// creazione utente del client
	char perm[PERM_SIZE];
	User *c_user = malloc(sizeof(User));
	if (c_user == NULL){
		perror("malloc");
		pthread_exit(NULL);
	}
	
	c_user->usr[0] = '\0';
	c_user->pwd[0] = '\0';
	
	// il client sceglie se registrarsi o loggarsi
	while (res != 0){
		handle(recv(c_sock, &net_choice, sizeof(net_choice), 0), c_sock, SERVER); // ricevo scelta del client
		choice = ntohl(net_choice);
		switch (choice){
			case 1:
				// REGISTRAZIONE
				handle(recv(c_sock, c_user->usr, USR_SIZE, 0), c_sock, SERVER); // client manda username
				handle(recv(c_sock, c_user->pwd, PWD_SIZE, 0), c_sock, SERVER); // client manda password
				handle(recv(c_sock, perm, PERM_SIZE, 0), c_sock, SERVER); // client manda permesso
				
				pthread_mutex_lock(&up_mutex);
				res = usrRegister(c_user, perm);
				pthread_mutex_unlock(&up_mutex);
				
				net_res = htonl(res);
				send(c_sock, &net_res, sizeof(net_res), 0);
				break;
			case 2:
				// LOGIN
				handle(recv(c_sock, c_user->usr, USR_SIZE, 0), c_sock, SERVER); // client manda username
				handle(recv(c_sock, c_user->pwd, PWD_SIZE, 0), c_sock, SERVER); // client manda password
				
				pthread_mutex_lock(&up_mutex);
				res = usrLogin(c_user);
				pthread_mutex_unlock(&up_mutex);
				
				net_res = htonl(res);
				send(c_sock, &net_res, sizeof(net_res), 0);
				break;
		}
	}
	
	// il client sceglie se aggiungere un contatto, cercare un contatto o uscire
	handle(recv(c_sock, &net_choice, sizeof(net_choice), 0), c_sock, SERVER); // client manda scelta
	choice = ntohl(net_choice); // conversione in host byte order
	while (choice != 3){
		switch (choice){
			case 1:
				// AGGIUNGI CONTATTO
				answer = checkPermission(c_user->usr, "w");
				// answer è bool (1 byte) quindi non serve convertirlo in network byte order
				send(c_sock, &answer, sizeof(answer), 0); // invia risultato al client
				if (answer){
					// accesso consentito
					char buffer[BUF_SIZE];
					handle(recv(c_sock, buffer, BUF_SIZE, 0), c_sock, SERVER); // il client invia contatto
					Contact *contatto = createContact(buffer);
					if (contatto == NULL){
						perror("malloc");
						pthread_exit(NULL);
					}
					memset(buffer, 0, BUF_SIZE); // azzero per poi memorizzare la risposta del server
					
					pthread_mutex_lock(&r_mutex);
					addContact(contatto, buffer); // aggiungo il contatto in rubrica
					pthread_mutex_unlock(&r_mutex);
					
					send(c_sock, buffer, BUF_SIZE, 0); // invio del risultato al client
					free(contatto); // evita memory leak
				}
				break;
			case 2:
				// CERCA CONTATTO
				answer = checkPermission(c_user->usr, "r");
				send(c_sock, &answer, sizeof(answer), 0); // invia risultato al client
				if (answer){
					// accesso consentito
					char buffer[BUF_SIZE];
					handle(recv(c_sock, buffer, BUF_SIZE, 0), c_sock, SERVER); // il client invia il contatto
					Contact *contatto = createContact(buffer);
					if (contatto == NULL){
						perror("malloc");
						pthread_exit(NULL);
					}
					memset(buffer, 0, BUF_SIZE);
					
					pthread_mutex_lock(&r_mutex);
					searchContact(contatto, buffer); // cerca il contatto in rubrica
					pthread_mutex_unlock(&r_mutex);
					
					send(c_sock, buffer, BUF_SIZE, 0); // invio del risultato al client
					free(contatto); // evita memory leak
				}
				break;
		}
		handle(recv(c_sock, &net_choice, sizeof(net_choice), 0), c_sock, SERVER);
		choice = ntohl(net_choice);
	}
	
	puts("client in uscita");
	close(c_sock);
	free(c_user);
	pthread_exit(NULL);
}

int parseCmdLine(int argc, char *argv[], char **sPort) {
	if (argc < 3){
		printf("Usage: %s -p (port) [-h]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	for (int i = 1; i < argc; i++){
		if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "-P")){
			*sPort = argv[i + 1];
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-H")){
			printf("Usage: %s -p (port) [-h]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	
	return 0;
}

