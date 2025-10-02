#include "helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

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

void safeFgets(char *buffer, size_t size){
    alarm(TIMER);
    if (fgets(buffer, size, stdin) == NULL) {
        alarm(0);
        puts("Errore nella scrittura o fine del file");
        exit(0);
    }
    
    alarm(0);
    if (strchr(buffer, '\n') != NULL){
    	buffer[strcspn(buffer, "\n")] = '\0';
    } else {
    	flushInput();
    }
    
    return;
}

void safeScanf(int *val){
	alarm(TIMER);
	if (scanf("%d", val) == 1){
		alarm(0);
		flushInput();
		return;
	} else {
		alarm(0);
		puts("Input non valido");
		flushInput();
		exit(0);
	}
}




