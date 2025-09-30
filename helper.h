/*
 * Progetto: Elenco Telefonico
 * Autore: Dario Sella
 * Corso: Sistemi Operativi
 * Data: Settembre 2025
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define BUF_SIZE (256)
#define NAME_SIZE (64)
#define NUMBER_SIZE (32)
#define USR_SIZE (64)
#define PWD_SIZE (64)
#define SERVER (0)
#define CLIENT (1)

void handle(int res, int sock, int who);
