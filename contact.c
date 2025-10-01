#include "contact.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

Contact *createContact(char *buffer){
	// funzione per la creazione di un nuovo contatto da inserire o cercare
	Contact *contatto = malloc(sizeof(Contact));
	if (contatto == NULL){
		perror("malloc");
		return NULL;
	}
	
	contatto->name[0] = '\0';
	contatto->number[0] = '\0';
	
	char *token = strtok(buffer, " \n");
	while (token != NULL){
		char *end;
		long num = strtol(token, &end, 10);
		
		if (*end == '\0'){
			// il token è il numero di telefono
			strcpy(contatto->number, token);
		} else {
			// mantiene la sintassi "nome [nomi secondari] cognome"
			strcat(contatto->name, token);
			strcat(contatto->name, " ");
		}
		
		token = strtok(NULL, " \n");
	}
	
	int namelen = strlen(contatto->name);
	contatto->name[namelen - 1] = '\0'; // toglie lo spazio finale
	
	return contatto;
}

void addContact(Contact *contatto, char *answer){
	// funzione per l'aggiunta di un nuovo contatto alla fine della rubrica
	int fd = open("rubrica", O_CREAT | O_WRONLY | O_APPEND, 0600);
	if (fd == -1){
		perror("open");
		strcpy(answer, "Errore nell'apertura della rubrica\n");
		return; // errore
	}
	
	// controllo che l'utente abbia inserito sia il nome che il numero
	if (contatto->name[0] == '\0' || contatto->number[0] == '\0'){
		strcpy(answer, "Nome o Numero non inseriti\n");
		close(fd);
		return;
	}
	
	char buffer[BUF_SIZE];
	snprintf(buffer, BUF_SIZE, "%s %s\n", contatto->name, contatto->number);
	
	// controllo se contatto già esiste
	char temp[BUF_SIZE];
	searchContact(contatto, temp);
	if (strcmp(buffer, temp) == 0){
		// se il contatto già esiste
		strcpy(answer, "Il contatto già esiste\n");
		close(fd);
		return;
	}
	write(fd, buffer, strlen(buffer));
	
	strcpy(answer, "Il contatto è stato aggiunto con successo\n");
	close(fd);
	return; // contatto aggiunto
}

void searchContact(Contact *contatto, char *answer){
	int fd = open("rubrica", O_RDONLY);
	if (fd == -1){
		perror("open");
		strcpy(answer, "Errore nell'apertura della rubrica\n");
		return;
	}

	char buffer[BUF_SIZE];
	int i = 0;
	char c;

	while (read(fd, &c, 1) == 1){
		buffer[i++] = c;\
		if (c == '\n'){
			buffer[i] = '\0';
			
			// temp contiene il contatto preso dalla rubrica
			Contact *temp = createContact(buffer);
			if (temp == NULL){
				perror("malloc");
				strcpy(answer, "Errore di memoria insufficente\n");
				return; // errore
			}
			
			if (strcmp(temp->name, contatto->name) == 0){
				strcpy(contatto->number, temp->number);
				
				snprintf(answer, BUF_SIZE, "%s %s\n", contatto->name, contatto->number); // contatto trovato
				
				free(temp);
				close(fd);
				return;
			}

			free(temp);
			memset(buffer, 0, BUF_SIZE);
			i = 0;
		}
	}
	
	strcpy(answer, "Il contatto non esiste\n"); // contatto non trovato
	close(fd);
	return;
}

