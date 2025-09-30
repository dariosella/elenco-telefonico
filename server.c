/*
 * Progetto: Elenco Telefonico
 * Autore: Dario Sella
 * Corso: Sistemi Operativi
 * Data: Settembre 2025
*/

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

#include "helper.h"
#include "contact.h"

#define LISTENQ (8)

typedef struct {
	char usr[USR_SIZE];
	char pwd[PWD_SIZE];
} User;

pthread_mutex_t fmutex;

void *clientThread(void *arg); // avvio thread

int parseCmdLine(int argc, char *argv[], char **sPort);
int login(User *utente);
bool checkPermission(char *filename, char *username, char *perm);

int main(int argc, char *argv[]){
	pthread_mutex_init(&fmutex, NULL); // inizializza mutex per lettura/scrittura su rubrica
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
	
	// login del client
	int res, net_res;
	User *c_user = malloc(sizeof(User));
	if (c_user == NULL){
		perror("malloc");
		pthread_exit(NULL);
	}
	
	handle(recv(c_sock, c_user->usr, USR_SIZE, 0), c_sock, SERVER); // client manda username
	handle(recv(c_sock, c_user->pwd, PWD_SIZE, 0), c_sock, SERVER); // client manda password
	while ( (res = login(c_user)) != 0){
		// login non andato a buon fine, ritenta
		net_res = htonl(res);
		send(c_sock, &net_res, sizeof(net_res), 0); // manda codice di errore al client
		handle(recv(c_sock, c_user->usr, USR_SIZE, 0), c_sock, SERVER);
		handle(recv(c_sock, c_user->pwd, PWD_SIZE, 0), c_sock, SERVER);
	}
	net_res = htonl(res);
	send(c_sock, &net_res, sizeof(net_res), 0); // manda codice di successo al client
	
	// acquisizione scelta del client
	int choice, net_choice;
	bool answer;
	handle(recv(c_sock, &net_choice, sizeof(net_choice), 0), c_sock, SERVER); // client manda scelta
	choice = ntohl(net_choice); // conversione in host byte order
	while (choice != 3){
		switch (choice){
			case 1:
				// AGGIUNGI CONTATTO
				answer = checkPermission("permessi", c_user->usr, "w");
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
					
					pthread_mutex_lock(&fmutex);
					addContact("rubrica", contatto, buffer); // aggiungo il contatto in rubrica
					pthread_mutex_unlock(&fmutex);
					
					send(c_sock, buffer, BUF_SIZE, 0); // invio del risultato al client
					free(contatto); // evita memory leak
				}
				break;
			case 2:
				// CERCA CONTATTO
				answer = checkPermission("permessi", c_user->usr, "r");
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
					
					pthread_mutex_lock(&fmutex);
					searchContact("rubrica", contatto, buffer); // cerca il contatto in rubrica
					pthread_mutex_unlock(&fmutex);
					
					send(c_sock, buffer, BUF_SIZE, 0); // invio del risultato al client
					free(contatto); // evita memory leak
				}
				break;
		}
		handle(recv(c_sock, &net_choice, sizeof(net_choice), 0), c_sock, SERVER);
		choice = ntohl(net_choice);
	}
	
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

int login(User *utente){
	// ogni riga di users è "username password\n"
	int fd = open("utenti", O_RDONLY);
	if (fd == -1){
		perror("open");
		return -1;
	}
	
	char buffer[BUF_SIZE];
	
	int i = 0;
	char c;
	while (read(fd, &c, 1) == 1){
		// leggo un carattere alla volta
		buffer[i++] = c;
		if (c == '\n'){
			// ho letto una riga: "username password\n"
			buffer[i] = '\0';
			char *username = strtok(buffer, " \n");
			char *password = strtok(NULL, " \n");
			
			if (username != NULL && password != NULL){
				if (strcmp(username, utente->usr) == 0 && strcmp(password, utente->pwd) == 0){
					close(fd);
					return 0; // utente riconosciuto
				} else if (strcmp(username, utente->usr) == 0 && strcmp(password, utente->pwd) != 0){
					close(fd);
					return 1; // password sbagliata
				}
			}
			
			memset(buffer, 0, BUF_SIZE);
			i = 0;
		}
	}
	
	close(fd);
	return 2; // utente non esiste
}

bool checkPermission(char *filename, char *username, char *perm){
	// ogni riga di permission è "username permission\n"
	int fd = open(filename, O_RDONLY);
	if (fd == -1){
		perror("open");
		return false;
	}
	
	char buffer[BUF_SIZE];
	
	int i = 0;
	char c;
	while (read(fd, &c, 1) == 1){
		buffer[i++] = c;
		if (c == '\n'){
			// ho letto una riga "username permission\n"
			buffer[i] = '\0';
			char *buffer_username = strtok(buffer, " \n");
			char *buffer_perm = strtok(NULL, " \n");
			if (buffer_username != NULL && buffer_perm != NULL){
				if (strcmp(username, buffer_username) == 0) {
					if (strstr(buffer_perm, perm) != NULL) {
						// l'utente ha il permesso
						close(fd);
						return true;
					} else {
						close(fd);
						return false;
						}
				}
			}
			
			memset(buffer, 0, BUF_SIZE);
			i = 0;
		}
	}
	
	close(fd);
	return false;
}
