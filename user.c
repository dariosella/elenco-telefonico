#include "user.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int usrLogin(User *utente){
	if (utente == NULL){
		puts("usrLogin args NULL");
		return -1;
	}
	
	// ogni riga di users è "username password\n"
	int fd = open("utenti", O_RDONLY);
	if (fd == -1){
		perror("open utenti");
		return -1;
	}
	
	char buffer[BUF_SIZE];
	
	int i = 0;
	char c;
	int r = 0;
	while ( (r = safeRead(fd, &c, 1)) == 1){
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
					return -2; // password sbagliata
				}
			}
			
			memset(buffer, 0, BUF_SIZE);
			i = 0;
		}
	}
	
	if (r == -1){
		perror("read utenti");
		return -1;
	}
	
	close(fd);
	return -3; // utente non esiste
}

int usrRegister(User *utente, char *perm){
	if (utente == NULL || perm == NULL){
		puts("usrRegister args NULL");
		return -1;
	}
	
	int fd = open("utenti", O_CREAT | O_RDWR | O_APPEND, 0600);
	if (fd == -1){
		perror("open utenti");
		return -1;
	}
	
	int fd2 = open("permessi", O_CREAT | O_WRONLY | O_APPEND, 0600);
	if (fd2 == -1){
		perror("open permessi");
		close(fd);
		return -1;
	}
	
	char buffer[BUF_SIZE];
	int i = 0;
	char c;
	int r = 0;
	
	// CHECK USERNAME GIÀ UTILIZZATO
	while ((r = safeRead(fd, &c, 1)) == 1){
		// leggo un carattere alla volta
		buffer[i++] = c;
		if (c == '\n'){
			// ho letto una riga: "username password\n"
			buffer[i] = '\0';
			char *username = strtok(buffer, " \n");
			char *password = strtok(NULL, " \n");
			
			if (username != NULL && utente->usr != NULL){
				if (strcmp(username, utente->usr) == 0){
					close(fd);
					close(fd2);
					return -2; // username già utilizzato
				}
			}
			
			memset(buffer, 0, BUF_SIZE);
			i = 0;
		}
	}
	
	if (r == -1){
		perror("read utenti");
		close(fd);
		close(fd2);
		return -1;
	}
	
	// scrittura "username password\n" sul file utenti
	memset(buffer, 0, BUF_SIZE);
	snprintf(buffer, BUF_SIZE, "%s %s\n", utente->usr, utente->pwd);
	if (safeWrite(fd, buffer, strlen(buffer)) == -1){
		perror("write utenti");
		close(fd);
		close(fd2);
		return -1;
	}
	
	// scrittura "username permission\n" sul file permessi
	memset(buffer, 0, BUF_SIZE);
	snprintf(buffer, BUF_SIZE, "%s %s\n", utente->usr, perm);
	if (safeWrite(fd2, buffer, strlen(buffer)) == -1){
		perror("write permessi");
		close(fd);
		close(fd2);
		return -1;
	}
	
	close(fd);
	close(fd2);
	return 0; // utente registrato
}

int checkPermission(char *username, char *perm){
	if (username == NULL || perm == NULL){
		puts("checkPermission args NULL");
		return -1;
	}

	// ogni riga di permission è "username permission\n"
	int fd = open("permessi", O_RDONLY);
	if (fd == -1){
		perror("open permessi");
		return -1;
	}
	
	char buffer[BUF_SIZE];
	int i = 0;
	char c;
	int r = 0;
	while ((r = safeRead(fd, &c, 1)) == 1){
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
						return 1; // true
					} else {
						close(fd);
						return 0; // false
					}
				}
			}
			
			memset(buffer, 0, BUF_SIZE);
			i = 0;
		}
	}
	
	if (r == -1){
		perror("read permessi");
		close(fd);
		return -1;
	}
	
	close(fd);
	return 0; // false
}

