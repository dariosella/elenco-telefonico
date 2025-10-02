#define TIMER 30
#define LISTENQ (8)
#define BUF_SIZE (256)
#define NAME_SIZE (64)
#define NUMBER_SIZE (32)
#define USR_SIZE (64)
#define PWD_SIZE (64)
#define PERM_SIZE (3)
#define SERVER (0)
#define CLIENT (1)

#include <stddef.h>

void handle(int res, int sock, int who);
void flushInput();
void safeFgets(char *buffer, size_t size);
void safeScanf(int *val);
