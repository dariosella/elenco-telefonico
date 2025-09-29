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
int addContact(char *filename, Contact *contatto);
int searchContact(char *filename, Contact *contatto);
