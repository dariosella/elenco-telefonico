#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdbool.h>

#include "header.h"

typedef struct {
	char usr[USR_SIZE];
	char pwd[PWD_SIZE];
} User;

typedef struct {
	char name[NAME_SIZE];
	char number[NUMBER_SIZE];
} Contact;

pthread_mutex_t fmutex;

void *clientThread(void *arg);

int parseCmdLine(int argc, char *argv[], char **sPort);
int login(User *utente);
bool checkPermission(char *filename, char *username, char *perm);

Contact *getContact(int sock);
int addContact(char *filename, Contact *contatto);
int searchContact(char *filename, Contact *contatto);

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
		*c_sockptr = c_sock; // copia separata da passare al thread
		
		if (pthread_create(&tid, NULL, clientThread, (void *)c_sockptr) < 0){
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
		
		pthread_detach(tid);
	}
}

void *clientThread(void *arg){
	int c_sock = *((int *)arg);
	free(arg); // evita memory leak
	
	// login del client
	int res, net_res;
	User *c_user = malloc(sizeof(User));
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
	handle(recv(c_sock, &net_choice, sizeof(net_choice), 0), c_sock, SERVER);
	choice = ntohl(net_choice);
	while (choice != 3){
		switch(choice){
			case 1:
				// check permessi
				answer = checkPermission("permissions", c_user->usr, "w");
				send(c_sock, &answer, sizeof(answer), 0); // invia risultato al client
				if (answer){
					// accesso consentito
					Contact *contatto = getContact(c_sock); // client invia il contatto
					
					pthread_mutex_lock(&fmutex);
					int res = addContact("rubrica", contatto); // aggiungo il contatto in rubrica
					pthread_mutex_unlock(&fmutex);
					
					switch (res){
						case 0:
							// contatto aggiunto
							char succ[BUF_SIZE] = "Contatto aggiunto\n";
							send(c_sock, succ, BUF_SIZE, 0);
							break;
						default:
							// errore
							char err[BUF_SIZE] = "Errore\n";
							send(c_sock, err, BUF_SIZE, 0);
							break;
					}
					free(contatto); // evita memory leak
				}
				break;
			case 2:
				// check permessi
				answer = checkPermission("permissions", c_user->usr, "r");
				send(c_sock, &answer, sizeof(answer), 0); // invia risultato al client
				if (answer){
					// accesso consentito
					Contact *contatto = getContact(c_sock); // client invia il contatto
					
					pthread_mutex_lock(&fmutex);
					int res = searchContact("rubrica", contatto); // cerca il contatto in rubrica
					pthread_mutex_unlock(&fmutex);
					
					switch (res){
						case 0:
							// contatto trovato
							char buffer[BUF_SIZE];
							snprintf(buffer, BUF_SIZE, "%s%s\n", contatto->name, contatto->number);
							send(c_sock, buffer, BUF_SIZE, 0); // invia contatto e numero al client
							break;
						case 1:
							// contatto non trovato
							char not[BUF_SIZE] = "Contatto non trovato\n";
							send(c_sock, not, BUF_SIZE, 0);
							break;
						default:
							// errore
							char err[BUF_SIZE] = "Errore\n";
							send(c_sock, err, BUF_SIZE, 0);
							break;
					}
					free(contatto); // evita memory leak
				}
				break;
		}
		handle(recv(c_sock, &net_choice, sizeof(net_choice), 0), c_sock, SERVER);
		choice = ntohl(net_choice);
	}
	
	puts("client esce, chiudo connessione");
	close(c_sock);
	free(c_user);
	pthread_exit(NULL);
}

int parseCmdLine(int argc, char *argv[], char **sPort){
	// funzione per argomenti corretti, acquisizione della porta su cui il server si mette in ascolto
	if (argc == 1){
		printf("Usage: %s -p (port) [-h]\n", argv[0]);
		exit(EXIT_SUCCESS);
	}
	
	for (int i = 1; i < argc; i++){
		if (!strncmp(argv[i], "-p", 2) || !strncmp(argv[i], "-P", 2)){
			*sPort = argv[i+1];
		} else if (!strncmp(argv[i], "-h", 2) || !strncmp(argv[i], "-H", 2)){
			printf("Usage: %s -p (port) [-h]\n", argv[0]);
			exit(EXIT_SUCCESS);
		}
	}
	
	return 0;
}

int login(User *utente){
	// ogni riga di users è "username password\n"
	int fd = open("users", O_RDONLY);
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
			
			if (username == NULL || password == NULL){
				memset(buffer, 0, BUF_SIZE);
				i = 0;
				continue;
			}
			
			if (strcmp(username, utente->usr) == 0 && strcmp(password, utente->pwd) == 0){
				close(fd);
				return 0; // utente riconosciuto
			} else if (strcmp(username, utente->usr) == 0 && strcmp(password, utente->pwd) != 0){
				close(fd);
				return 1; // password sbagliata
			}
			
			memset(buffer, 0, BUF_SIZE);
			i = 0;
		}
	}
	
	close(fd);
	return 2; // utente non presente
}

bool checkPermission(char *filename, char *username, char *perm){
	// ogni riga di permission è "username permission\n"
	int fd = open(filename, O_RDONLY);
	char buffer[BUF_SIZE];
	
	int i = 0;
	char c;
	while (read(fd, &c, 1) == 1){
		buffer[i++] = c;
		if (c == '\n'){
			buffer[i] = '\0';
			char *buffer_username = strtok(buffer, " \n");
			char *buffer_perm = strtok(NULL, " \n");
			if (strcmp(username, buffer_username) == 0) {
				if (strstr(buffer_perm, perm) != NULL || strcmp(buffer_perm, "rw") == 0) {
					close(fd);
					return true;
				} else {
					close(fd);
					return false;
			    	}
			}
			
			memset(buffer, 0, BUF_SIZE);
			i = 0;
		}
	}
	
	close(fd);
	return false;
}

Contact *getContact(int sock){
	// funzione per la creazione di un nuovo contatto da inserire o cercare
	Contact *contatto = malloc(sizeof(Contact));
	
	// il client invia il nuovo contatto
	char buffer[BUF_SIZE];
	handle(recv(sock, buffer, BUF_SIZE, 0), sock, SERVER);
	
	char *token = strtok(buffer, " \n");
	while (token != NULL){
		char *end;
		long num = strtol(token, &end, 10);
		
		if (*end == '\0'){
			// il token è il numero di telefono
			strcpy(contatto->number, token);
		} else {
			// mantiene la sintassi "nome [nomi secondari] cognome "
			strcat(contatto->name, token);
			strcat(contatto->name, " ");
		}
		
		token = strtok(NULL, " \n");
	}
	
	return contatto;
}

int addContact(char *filename, Contact *contatto){
	// funzione per l'aggiunta di un nuovo contatto alla fine della rubrica
	int fd = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0600);
	if (fd == -1){
		perror("open");
		return -1;
	}
	
	char buffer[BUF_SIZE];
	snprintf(buffer, BUF_SIZE, "%s%s\n", contatto->name, contatto->number);
	write(fd, buffer, strlen(buffer));
	
	close(fd);
	return 0;
}

int searchContact(char *filename, Contact *contatto){
	int fd = open(filename, O_RDONLY);
	if (fd == -1){
		perror("open");
		return -1;
	}

	char buffer[BUF_SIZE];
	int i = 0;
	char c;

	while (read(fd, &c, 1) == 1){
		buffer[i++] = c;
		if (c == '\n'){
			buffer[i] = '\0';

			if (strncmp(buffer, contatto->name, strlen(contatto->name)) == 0){
				// Cerca numero
				char *token = strtok(buffer, " \n");
				while (token != NULL){
					char *end;
					long num = strtol(token, &end, 10);
					
					if (*end == '\0'){
						strcpy(contatto->number, token);
						close(fd);
						return 0; // contatto trovato
					}
					token = strtok(NULL, " \n");
				}
			}

			memset(buffer, 0, BUF_SIZE);
			i = 0;
		}
	}

	close(fd);
	return 1; // non trovato
}
