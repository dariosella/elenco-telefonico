#include <sys/socket.h>
#include <sys/types.h>
#include <acl/libacl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>

#define LISTENQ (8)
#define BUF_SIZE (128)
#define NAME_SIZE (64)
#define NUMBER_SIZE (32)

typedef struct {
	char name[NAME_SIZE];
	char number[NUMBER_SIZE];
} Contact;

pthread_mutex_t fmutex;

void *clientThread(void *arg);

int parseCmdLine(int argc, char *argv[], char **sPort);
int checkPermission(char *filename, uid_t uid, int perm);

Contact *getContact(int sock);
void addContact(char *filename, Contact *contatto);
void searchContact(char *filename, Contact *contatto);

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

void *clientThread(void *arg){
	int c_sock = *((int *)arg);
	free(arg); // evita memory leak
	int choice, answer;
	
	// acquisizone uid del client
	uid_t c_uid;
	recv(c_sock, &c_uid, sizeof(c_uid), 0);
	
	// acquisizione scelta del client
	recv(c_sock, &choice, sizeof(choice), 0);
	while (choice != 3){
		switch(choice){
			case 1:
				// check permessi
				answer = checkPermission("rubrica", c_uid, ACL_WRITE);
				send(c_sock, &answer, sizeof(answer), 0);
				if (answer){
					// accesso consentito
					Contact *contatto = getContact(c_sock);
					
					pthread_mutex_lock(&fmutex);
					addContact("rubrica", contatto);
					pthread_mutex_unlock(&fmutex);
					
					free(contatto); // evita memory leak
				}
				break;
			case 2:
				// check permessi
				answer = checkPermission("rubrica", c_uid, ACL_READ);
				send(c_sock, &answer, sizeof(answer), 0);
				if (answer){
					// accesso consentito
					Contact *contatto = getContact(c_sock);
					
					pthread_mutex_lock(&fmutex);
					searchContact("rubrica", contatto);
					pthread_mutex_unlock(&fmutex);
					
					if (contatto->number[0] != '\0'){
						// contatto trovato
						char buffer[BUF_SIZE];
						snprintf(buffer, BUF_SIZE, "%s%s\n", contatto->name, contatto->number);
						send(c_sock, buffer, strlen(buffer), 0);
					} else {
						// contatto non trovato
						char *buffer = "contatto non trovato";
						send(c_sock, buffer, strlen(buffer), 0);
					}
					
					free(contatto); // evita memory leak
				}
				break;
			default:
				puts("Invalid choice");
		}
		recv(c_sock, &choice, sizeof(choice), 0);
	}
	
	close(c_sock);
	pthread_exit(NULL);
}

int checkPermission(char *filename, uid_t uid, int perm) {
	// funzione per il controllo dei permessi ACL r/w sulla rubrica dell'utente (client) specifico
    acl_t acl = acl_get_file(filename, ACL_TYPE_ACCESS);
    if (acl == NULL) {
        perror("acl_get_file");
        return 0;
    }

    acl_entry_t entry;
    int entry_id = ACL_FIRST_ENTRY;
    while (acl_get_entry(acl, entry_id, &entry) == 1) {
        entry_id = ACL_NEXT_ENTRY;
        acl_tag_t tag;
        acl_get_tag_type(entry, &tag);

        if (tag == ACL_USER) {
            uid_t *entry_uid = (uid_t *)acl_get_qualifier(entry);
            if (*entry_uid == uid) {
                acl_permset_t permset;
                acl_get_permset(entry, &permset);
                int has_perm = acl_get_perm(permset, perm);
                acl_free(entry_uid);
                acl_free(acl);
                return has_perm;
            }
            acl_free(entry_uid);
        }
    }
    acl_free(acl);
    return 0; // Nessuna entry trovata per quell'UID
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

void addContact(char *filename, Contact *contatto){
	// funzione per l'aggiunta di un nuovo contatto alla fine della rubrica
	int fd = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0600);
	if (fd == -1){
		perror("open");
		exit(EXIT_FAILURE);
	}
	
	char buffer[BUF_SIZE];
	snprintf(buffer, BUF_SIZE, "%s%s\n", contatto->name, contatto->number);
	write(fd, buffer, strlen(buffer));
	
	close(fd);
}

void searchContact(char *filename, Contact *contatto){
	// funzione per la ricerca di un contatto in rubrica
	int fd = open(filename, O_RDONLY);
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
						return;	
					}
				}
			}
			
			memset(filelen, 0, BUF_SIZE); // reset del buffer
			i = 0; // riparto dall'inizio del buffer
		}
	}
	
	contatto->number[0] = '\0'; // contatto non trovato
	close(fd);
}



