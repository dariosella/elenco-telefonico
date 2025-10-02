#include "helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

void handle(int res, int sock, int who){
	if (res == 0){
		// la connessione è stata chiusa
		close(sock);
		if (who == SERVER){
			puts("Connessione chiusa");
			pthread_exit(NULL);
		} else if (who == CLIENT){
			puts("Connessione chiusa");
			exit(EXIT_SUCCESS);
		}
	} else if (res == -1){
		// errore
		perror("recv");
		if (who == SERVER){
			pthread_exit(NULL);
		} else if (who == CLIENT){
			exit(EXIT_FAILURE);
		}
	}
}

void flushInput(){
	char c;
	while ( (c = getchar() != '\n') && c != EOF);
}

void checkInput(char *input){
	if (input == NULL){
		puts("Errore nella scrittura o fine del file");
		pthread_exit(NULL);
	}
}




