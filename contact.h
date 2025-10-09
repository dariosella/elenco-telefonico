#include "helper.h"

typedef struct {
	char name[NAME_SIZE];
	char number[NUMBER_SIZE];
} Contact;

Contact *createContact(char *buffer);
int addContact(Contact *contatto, char *answer);
int searchContact(Contact *contatto, char *answer);
