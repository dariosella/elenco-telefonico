#include "contact.h"

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

int addContact(char *filename, Contact *contatto){
	// funzione per l'aggiunta di un nuovo contatto alla fine della rubrica
	int fd = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0600);
	if (fd == -1){
		perror("open");
		return -1; // errore
	}
	
	// controllo che l'utente abbia inserito sia il nome che il numero
	if (contatto->name[0] == '\0' || contatto->number[0] == '\0'){
		close(fd);
		return 1;
	}
	
	// controllo se contatto è già presente in rubrica
	if (searchContact("rubrica", contatto) == 0){
		close(fd);
		return 2;
	}
	
	char buffer[BUF_SIZE];
	snprintf(buffer, BUF_SIZE, "%s %s\n", contatto->name, contatto->number);
	write(fd, buffer, strlen(buffer));
	
	close(fd);
	return 0; // contatto aggiunto
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
			
			// temp contiene il contatto preso dalla rubrica
			Contact *temp = createContact(buffer);
			if (temp == NULL){
				perror("malloc");
				return -1; // errore
			}
			
			if (strcmp(temp->name, contatto->name) == 0){
				strcpy(contatto->number, temp->number);
				free(temp);
				close(fd);
				return 0; // contatto trovato
			}

			free(temp);
			memset(buffer, 0, BUF_SIZE);
			i = 0;
		}
	}
	
	close(fd);
	return 1; // non trovato
}
