#include "helper.h"

typedef struct {
	char name[NAME_SIZE];
	char number[NUMBER_SIZE];
} Contact;

Contact *createContact(char *buffer);
void addContact(Contact *contatto, char *answer);
void searchContact(Contact *contatto, char *answer);
