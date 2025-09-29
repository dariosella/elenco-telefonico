#include "helper.h"

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
