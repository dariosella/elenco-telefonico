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
	int choice, res;
	bool answer;
	
	// login del client
	User *c_user = malloc(sizeof(User));
	recv(c_sock, c_user->usr, USR_SIZE, 0); // client manda username
	recv(c_sock, c_user->pwd, PWD_SIZE, 0); // client manda password
	while ( (res = login(c_user)) != 0){
		// login non andato a buon fine, ritenta
		send(c_sock, &res, sizeof(res), 0); // manda codice di errore al client
		recv(c_sock, c_user->usr, USR_SIZE, 0);
		recv(c_sock, c_user->pwd, PWD_SIZE, 0);
	}
	send(c_sock, &res, sizeof(res), 0); // manda codice di successo al client
	
	// acquisizione scelta del client
	recv(c_sock, &choice, sizeof(choice), 0);
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
							char *succ = "Contatto aggiunto\n";
							send(c_sock, succ, strlen(succ) + 1, 0);
							break;
						default:
							// errore
							char *err = "Errore\n";
							send(c_sock, err, strlen(err) + 1, 0);
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
							send(c_sock, buffer, strlen(buffer), 0); // invia contatto e numero al client
							break;
						case 1:
							// contatto non trovato
							char *not = "Contatto non trovato\n";
							send(c_sock, not, strlen(not) + 1, 0);
							break;
						default:
							// errore
							char *err = "Errore\n";
							send(c_sock, err, strlen(err) + 1, 0);
							break;
					}
					free(contatto); // evita memory leak
				}
				break;
		}
		recv(c_sock, &choice, sizeof(choice), 0);
	}
	
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
			buffer[i] = '\0'; // controllare
			char *username = strtok(buffer, " \n");
			char *password = strtok(NULL, " \n");
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
	contatto->name[0] = '\0';
	contatto->number[0] = '\0';
	
	// il client invia il nuovo contatto
	char buffer[BUF_SIZE];
	recv(sock, buffer, BUF_SIZE, 0);
	
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
	// funzione per la ricerca di un contatto in rubrica
	int fd = open(filename, O_RDONLY);
	if (fd == -1){
		perror("open");
		return -1;
	}
	
	char name[NAME_SIZE];
	char filelen[BUF_SIZE];
	
	// leggiamo un carattere alla volta
	char c;
	int i = 0;
	while (read(fd, &c, 1) == 1){
		filelen[i++] = c;
		if (c == '\n'){
			strcpy(name, contatto->name);
			char *token = strtok(filelen, " \n");
			char *token_name = strtok(name, " ");
			while (token != NULL && token_name != NULL && strcmp(token, token_name) == 0){
				token = strtok(NULL, " \n");
				token_name = strtok(NULL, " ");
			}
			
			if (token_name == NULL){
				// corrispondenza possibile
				if (token != NULL){
					char *end;
					long num = strtol(token, &end, 10);
					
					if (*end == '\0'){
						// il token è il numero di telefono, corrispondenza trovata
						strcpy(contatto->number, token);
						close(fd);
						return 0;
					}
				}
			}
			
			memset(filelen, 0, BUF_SIZE); // reset del buffer
			i = 0; // riparto dall'inizio del buffer
		}
	}
	
	contatto->number[0] = '\0'; // contatto non trovato
	close(fd);
	return 1;
}
