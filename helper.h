#define TIMEOUT 30
#define LISTENQ (8)
#define BUF_SIZE (256)
#define NAME_SIZE (64)
#define NUMBER_SIZE (32)
#define USR_SIZE (64)
#define PWD_SIZE (64)
#define PERM_SIZE (16)
#define CHOICE_SIZE (16)

#include <sys/types.h>

ssize_t safeRecv(int sfd, void *buffer, size_t size, int flags);
ssize_t safeSend(int sfd, const void *buffer, size_t size, int flags);
ssize_t safeRead(int fd, void *buffer, size_t size);
ssize_t safeWrite(int fd, const void *buffer, size_t size);
