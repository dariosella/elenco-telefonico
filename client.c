#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BUF_SIZE (256)
#define NAME_SIZE (64)
#define NUMBER_SIZE (32)
#define USR_SIZE (64)
#define PWD_SIZE (64)

int parseCmdLine(int argc, char *argv[], char **sAddr, char **sPort);

int main(int argc, char *argv[]){
	int s_sock, s_sock_len;
	struct sockaddr_in server;
	struct hostent *hp;
	
	char *sAddr, *sPort, *end;
	parseCmdLine(argc, argv, &sAddr, &sPort);
	
	int port = strtol(sPort, &end, 0);
	if (*end){
		puts("porta non riconosciuta");
		exit(EXIT_FAILURE);
	}
	
	if ( (s_sock = socket(AF_INET, SOCK_STREAM, 0) ) < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	if (inet_aton(sAddr, &server.sin_addr) <= 0){
		perror("inet_aton");
		if ( (hp = gethostbyname(sAddr)) == NULL){
			perror("gethostbyname");
			exit(EXIT_FAILURE);
		}
		server.sin_addr = *((struct in_addr *)hp->h_addr);
	}
	
	if ( connect(s_sock, (struct sockaddr *)&server, sizeof(server)) < 0){
		perror("connect");
		exit(EXIT_FAILURE);
	}
	
	// client connesso
	
	// login del client
	char username[USR_SIZE];
	char password[PWD_SIZE];
	int res = -1; // gestire meglio res
	
	while (res != 0){
		printf("%s", "Inserisci username: ");
		fgets(username, USR_SIZE, stdin);
		username[strcspn(username, "\n")] = '\0';
		
		printf("%s", "Inserisci password: ");
		fgets(password, PWD_SIZE, stdin);
		password[strcspn(password, "\n")] = '\0';
		
		send(s_sock, username, strlen(username) + 1, 0); // invia username al server
		send(s_sock, password, strlen(password) + 1, 0); // invia password al server
		
		recv(s_sock, &res, sizeof(res), 0); // riceve il risultato del login
		switch (res){
			case 1:
				puts("Password sbagliata");
				break;
			case 2:
				printf("%s Non esiste\n", username);
				break;
		}
	}
	printf("Benvenuto %s!\n", username);
	
	int choice;
	bool answer;
	printf("Scegliere:\n1. Aggiungere contatto\n2. Cercare contatto\n3. Uscire");
	scanf(" %d", &choice); // fflush stdin da inserire
	while (choice != 3){
		switch (choice){
			case 1:
				send(s_sock, &choice, sizeof(choice), 0);
				recv(s_sock, &answer, sizeof(answer), 0);
				if (answer){
					char buffer[BUF_SIZE];
					printf("%s", "Inserisci 'Nome [Nomi secondari] Cognome Numero'\n");
					fgets(buffer, BUF_SIZE, stdin);
					send(s_sock, buffer, BUF_SIZE, 0);
					// aggiungere risposta dal sever di SUCCESSO o FALLIMENTO
					
				} else {
					puts("Non hai i permessi necessari per aggiungere il contatto");
				}
				break;
			case 2:
				send(s_sock, &choice, sizeof(choice), 0);
				recv(s_sock, &answer, sizeof(answer), 0);
				if (answer){
					char buffer[BUF_SIZE];
					printf("%s", "Inserisci 'Nome [Nomi secondari] Cognome'\n");
					fgets(buffer, BUF_SIZE, stdin);
					send(s_sock, buffer, BUF_SIZE, 0);
					
					// il server invia il contatto completo, altrimenti 'contatto non trovato'
					recv(s_sock, buffer, BUF_SIZE, 0);
					printf("%s", buffer);
				} else {
					puts("Non hai i permessi necessari per cercare il contatto");
				}
				break;
			case 3:
				send(s_sock, &choice, sizeof(choice), 0);
				break;
			default:
				puts("Scelta non valida");
		}
		printf("Scegliere:\n1. Aggiungere contatto\n2. Cercare contatto\n3. Uscire");
		scanf(" %d", &choice);
	}
	
	close(s_sock);
	exit(EXIT_SUCCESS);
}

int parseCmdLine(int argc, char *argv[], char **sAddr, char **sPort){
	int n = 1;
	
	while (n < argc){
		if ( !strncmp(argv[n], "-a", 2) || !strncmp(argv[n], "-A", 2) ){
			*sAddr = argv[++n]; // salva indirizzo o nome host
		} else if ( !strncmp(argv[n], "-p", 2) || !strncmp(argv[n], "-P", 2) ){
			*sPort = argv[++n]; // salva porta
		} else if ( !strncmp(argv[n], "-h", 2) || !strncmp(argv[n], "-H", 2) ){
			printf("Usage: %s -a (indirizzo remoto) -p (porta remota) [-h]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
		++n;
	}
	
	if (argc == 1){
			printf("Usage: %s -a (indirizzo remoto) -p (porta remota) [-h]\n", argv[0]);
			exit(EXIT_FAILURE);
	}
	
	return 0;
}











