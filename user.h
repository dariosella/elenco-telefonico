#include "helper.h"

typedef struct {
	char usr[USR_SIZE];
	char pwd[PWD_SIZE];
	char prm[PERM_SIZE];
} User;

int usrLogin(User *utente);
int usrRegister(User *utente);
int checkPermission(char *username, char *perm);
