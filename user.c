#include "user.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>

int usrLogin(User *utente){
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

int usrRegister(User *utente, char *perm){
	int fd = open("utenti", O_CREAT | O_RDWR | O_APPEND, 0600);
	int fd2 = open("permessi", O_CREAT | O_WRONLY | O_APPEND, 0600);
	if (fd == -1 || fd2 == -1){
		perror("open");
		return -1;
	}
	
	char buffer[BUF_SIZE];
	int i = 0;
	char c;
	// controllo se lo username è già utilizzato
	while (read(fd, &c, 1) == 1){
		// leggo un carattere alla volta
		buffer[i++] = c;
		if (c == '\n'){
			// ho letto una riga: "username password\n"
			buffer[i] = '\0';
			char *username = strtok(buffer, " \n");
			char *password = strtok(NULL, " \n");
			
			if (username != NULL){
				if (strcmp(username, utente->usr) == 0){
					close(fd);
					close(fd2);
					return 1; // username già utilizzato
				}
			}
			
			memset(buffer, 0, BUF_SIZE);
			i = 0;
		}
	}
	
	// scrittura "username password\n" sul file utenti
	memset(buffer, 0, BUF_SIZE);
	snprintf(buffer, BUF_SIZE, "%s %s\n", utente->usr, utente->pwd);
	write(fd, buffer, strlen(buffer));
	
	// scrittura "username permission\n" sul file permessi
	memset(buffer, 0, BUF_SIZE);
	snprintf(buffer, BUF_SIZE, "%s %s\n", utente->usr, perm);
	write(fd2, buffer, strlen(buffer));
	
	close(fd);
	close(fd2);
	return 0; // utente registrato
}

bool checkPermission(char *username, char *perm){
	// ogni riga di permission è "username permission\n"
	int fd = open("permessi", O_RDONLY);
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

