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

#include "helper.h"

int parseCmdLine(int argc, char *argv[], char **sAddr, char **sPort);

int main(int argc, char *argv[]){
	int s_sock;
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
	char username[USR_SIZE];
	char password[PWD_SIZE];
	char perm[PERM_SIZE];
	int choice, net_choice;
	int res = -1, net_res = -1;
	bool answer;
	
	// scegliere se loggare o registrarsi
	while (res != 0){
		printf("%s", "Scegliere se:\n1. Registrarti\n2. Loggarti\n");
		scanf("%d", &choice);
		while (getchar() != '\n'); // fflush stdin
		net_choice = htonl(choice);
		send(s_sock, &net_choice, sizeof(net_choice), 0);
		switch (choice){
			case 1:
				// REGISTRAZIONE
				memset(username, 0, USR_SIZE);
				memset(password, 0, PWD_SIZE);
				memset(perm, 0, PERM_SIZE);
				
				printf("%s", "Inserisci username: ");
				fgets(username, USR_SIZE, stdin);
				username[strcspn(username, "\n")] = '\0';
				
				printf("%s", "Inserisci password: ");
				fgets(password, PWD_SIZE, stdin);
				password[strcspn(password, "\n")] = '\0';
				
				printf("%s", "Inserisci un permesso tra:\nlettura - 'r'\nscrittura - 'w'\nlettura e scrittura - 'rw'\n");
				fgets(perm, PERM_SIZE, stdin);
				perm[strcspn(perm, "\n")] = '\0';
				
				send(s_sock, username, USR_SIZE, 0); // invia username al server
				send(s_sock, password, PWD_SIZE, 0); // invia password al server
				send(s_sock, perm, PERM_SIZE, 0); // invia permesso al server
				
				handle(recv(s_sock, &net_res, sizeof(net_res), 0), s_sock, CLIENT); // riceve il risultato
				res = ntohl(net_res);
				switch (res){
					case 1:
						printf("%s già utilizzato\n", username);
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
				
				printf("%s", "Inserisci username: ");
				fgets(username, USR_SIZE, stdin);
				username[strcspn(username, "\n")] = '\0';
				
				printf("%s", "Inserisci password: ");
				fgets(password, PWD_SIZE, stdin);
				password[strcspn(password, "\n")] = '\0';
				
				send(s_sock, username, USR_SIZE, 0); // invia username al server
				send(s_sock, password, PWD_SIZE, 0); // invia password al server
				
				handle(recv(s_sock, &net_res, sizeof(net_res), 0), s_sock, CLIENT); // riceve il risultato
				res = ntohl(net_res);
				switch (res){
					case 1:
						puts("Password sbagliata");
						break;
					case 2:
						printf("%s Non esiste\n", username);
						break;
					case -1:
						puts("Errore");
						break;
				}
				break;
		}
	}
	
	printf("Benvenuto %s!\n", username);
	printf("%s", "Scegliere:\n1. Aggiungere contatto\n2. Cercare contatto\n3. Uscire\n");
	scanf("%d", &choice);
	while (getchar() != '\n'); // fflush stdin
	
	while (choice != 3){
		switch (choice){
			case 1:
				net_choice = htonl(choice);
				send(s_sock, &net_choice, sizeof(net_choice), 0);
				// il server mi dice se ho i permessi necessari
				handle(recv(s_sock, &answer, sizeof(answer), 0), s_sock, CLIENT);
				if (answer){
					char buffer[BUF_SIZE];
					printf("%s", "Inserisci 'Nome [Nomi secondari] Cognome Numero'\n");
					fgets(buffer, BUF_SIZE, stdin);
					send(s_sock, buffer, BUF_SIZE, 0);
					memset(buffer, 0, BUF_SIZE);
					// risposta dal sever di successo o fallimento
					handle(recv(s_sock, buffer, BUF_SIZE, 0), s_sock, CLIENT);
					printf("%s", buffer);
					
				} else {
					puts("Non hai i permessi necessari per aggiungere il contatto");
				}
				break;
			case 2:
				net_choice = htonl(choice);
				send(s_sock, &net_choice, sizeof(net_choice), 0);
				// il server mi dice se ho i permessi necessari
				handle(recv(s_sock, &answer, sizeof(answer), 0), s_sock, CLIENT);
				if (answer){
					char buffer[BUF_SIZE];
					printf("%s", "Inserisci 'Nome [Nomi secondari] Cognome'\n");
					fgets(buffer, BUF_SIZE, stdin);
					send(s_sock, buffer, BUF_SIZE, 0);
					
					// il server invia il contatto e numero
					memset(buffer, 0, BUF_SIZE);
					handle(recv(s_sock, buffer, BUF_SIZE, 0), s_sock, CLIENT);
					printf("%s", buffer);
				} else {
					puts("Non hai i permessi necessari per cercare il contatto");
				}
				break;
			default:
				puts("Scelta non valida");
		}
		printf("%s", "Scegliere:\n1. Aggiungere contatto\n2. Cercare contatto\n3. Uscire\n");
		scanf("%d", &choice);
		while (getchar() != '\n'); // fflush stdin
	}
	
	net_choice = htonl(choice);
	send(s_sock, &net_choice, sizeof(net_choice), 0); // se choice è 3
	close(s_sock);
	exit(EXIT_SUCCESS);
}

int parseCmdLine(int argc, char *argv[], char **sAddr, char **sPort){
	if (argc < 5){
		printf("Usage: %s -a (indirizzo remoto) -p (porta remota) [-h]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	for (int i = 0; i < argc; i++){
		if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "-A")){
			*sAddr = argv[i + 1];
		} else if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "-P")){
			*sPort = argv[i + 1];
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-H")){
			printf("Usage: %s -a (indirizzo remoto) -p (porta remota) [-h]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}











