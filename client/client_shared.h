#ifndef CLIENT_SHARED_H
#define CLIENT_SHARED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>

#define PORT 9000
#define AES_ENCRYPT 0
#define AES_DECRYPT 1

typedef struct {
    char cmd[20];
    char arg1[50];
    char arg2[50];
    char data[256];
} Packet;

// Function declarations
void auto_load_driver();
void crypt_msg(char *msg, int mode);
int connect_to_server(const char *ip);

#endif
