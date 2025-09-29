#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "helper.h"

typedef struct {
	char name[NAME_SIZE];
	char number[NUMBER_SIZE];
} Contact;

Contact *createContact(char *buffer);
void addContact(char *filename, Contact *contatto, char *answer);
void searchContact(char *filename, Contact *contatto, char *answer);
