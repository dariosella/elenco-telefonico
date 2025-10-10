#include "helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>

ssize_t safeRecv(int sfd, void *buffer, size_t size, int flags){
	ssize_t total = 0;
	char *ptr = buffer;

	while (total < size){
		ssize_t r = recv(sfd, ptr + total, size - total, flags);
		if (r == -1){
			if (errno == EINTR){
				continue; // riprova
			} else if (errno == EAGAIN || errno == EWOULDBLOCK){
				return -2; // timeout
			}
			else {
				return -1; // errore
			}
		} else if (r == 0){
			return -3; // connessione chiusa
		}
		total += r;
	}
	return total;
}

ssize_t safeSend(int sfd, const void *buffer, size_t size, int flags){
	ssize_t total = 0;
	const char *ptr = buffer;
	
	while (total < size){
		ssize_t r = send(sfd, ptr + total, size - total, flags);
		if (r == -1){
			if (errno == EINTR){
				continue; // riprova
			} else if (errno == EPIPE){
				return -3; // connessione chiusa
			} else if (errno == EAGAIN || errno == EWOULDBLOCK){
				return -2; // timeout
			} else {
				return -1; // errore
			}
		}
		total += r;
	}
	return total;
}

ssize_t safeRead(int fd, void *buffer, size_t size){
	ssize_t total = 0;
	char *ptr = buffer;
	
	while (total < size){
		ssize_t r = read(fd, ptr + total, size - total);
		if (r == -1){
			if (errno == EINTR){
				continue; // riprova
			} else {
				return -1; // errore
			}
		} else if (r == 0){
			break; // EOF
		}
		total += r;
	}
	return total;
}

ssize_t safeWrite(int fd, const void *buffer, size_t size){
	ssize_t total = 0;
	const char *ptr = buffer;
	
	while (total < size){
		ssize_t r = write(fd, ptr + total, size - total);
		if (r == -1){
			if (errno == EINTR){
				continue; // riprova
			} else {
				return -1; // errore
			}
		}
		total += r;
	}
	return total;
}

ssize_t readLine(int fd, char *line, size_t size) {
    size_t i = 0;
    ssize_t r;
    char c;

    while (i < size - 1) {
        r = safeRead(fd, &c, 1);
        if (r == 1) {
            line[i++] = c;
            if (c == '\n') {
                line[i] = '\0';
                return i; // OK
            }
        } else if (r == 0) {
            // EOF
            line[i] = '\0';
            return i;
        } else {
            perror("read");
            return -1;
        }
    }
    
    line[size - 1] = '\0';
    return -2; // linea troppo lunga
}

