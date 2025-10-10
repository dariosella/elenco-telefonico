#include "contact.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

// CREA UN CONTATTO
Contact *createContact(char *buffer){
	Contact *contatto = malloc(sizeof(Contact));
	if (contatto == NULL){
		perror("malloc contact");
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

// AGGIUNGI UN CONTATTO IN RUBRICA
int addContact(Contact *contatto, char *answer){
	if (contatto == NULL || answer == NULL){
		puts("addContact args NULL");
		return -1;
	}
	
	int fd = open("rubrica", O_CREAT | O_WRONLY | O_APPEND, 0600);
	if (fd == -1){
		perror("open rubrica");
		return -1;
	}
	
	if (contatto->name[0] == '\0' || contatto->number[0] == '\0'){
		strcpy(answer, "Nome o Numero non inseriti\n");
		close(fd);
		return 0;
	}
	
	char buffer[BUF_SIZE];
	snprintf(buffer, BUF_SIZE, "%s %s\n", contatto->name, contatto->number);
	
	char temp[BUF_SIZE];
	searchContact(contatto, temp);
	if (strcmp(buffer, temp) == 0){
		strcpy(answer, "Il contatto già esiste\n");
		close(fd);
		return 0;
	}
	if (safeWrite(fd, buffer, strlen(buffer)) == -1){
		perror("write rubrica");
		close(fd);
		return -1;
	}
	
	strcpy(answer, "Il contatto è stato aggiunto con successo\n");
	close(fd);
	return 0;
}

// CERCA UN CONTATTO IN RUBRICA
int searchContact(Contact *contatto, char *answer){
	int fd = open("rubrica", O_RDONLY);
	if (fd == -1){
		perror("open rubrica");
		return -1;
	}

	char buffer[BUF_SIZE];
	ssize_t r = 0;
	
	while ( (r = readLine(fd, buffer, BUF_SIZE)) > 0){
		Contact *temp = createContact(buffer);
		if (temp == NULL){
			perror("malloc contact");
			close(fd);
			return -1;
		}
		
		if (strcmp(temp->name, contatto->name) == 0){
			strcpy(contatto->number, temp->number);
			snprintf(answer, BUF_SIZE, "%s %s\n", contatto->name, contatto->number); // trovato
			
			free(temp);
			close(fd);
			return 0;
		}
		
		free(temp);
		memset(buffer, 0, BUF_SIZE);
	}
	
	if (r == -1){
		puts("read rubrica");
		close(fd);
		return -1;
	} else if (r == -2){
		puts("rubrica overflow");
		close(fd);
		return -1;
	}
	
	strcpy(answer, "Il contatto non esiste\n"); // non trovato
	close(fd);
	return 0;
}

