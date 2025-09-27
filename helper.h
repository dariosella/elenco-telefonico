#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

#define LISTENQ (8)
#define BUF_SIZE (256)
#define NAME_SIZE (64)
#define NUMBER_SIZE (32)
#define USR_SIZE (64)
#define PWD_SIZE (64)
#define SERVER (0)
#define CLIENT (1)

typedef struct {
	char usr[USR_SIZE];
	char pwd[PWD_SIZE];
} User;

typedef struct {
	char name[NAME_SIZE];
	char number[NUMBER_SIZE];
} Contact;

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
