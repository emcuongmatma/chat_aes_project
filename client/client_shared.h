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

// Constants
#define PORT 9000
#define AES_ENCRYPT 0
#define AES_DECRYPT 1

// Commands
#define CMD_LOGIN       "LOGIN"
#define CMD_LOGIN_OK    "LOGIN_OK"
#define CMD_REGISTER    "REGISTER"
#define CMD_REGISTER_OK "REGISTER_OK"
#define CMD_CREATE      "CREATE"
#define CMD_CREATE_OK   "CREATE_OK"
#define CMD_JOIN        "JOIN"
#define CMD_JOIN_OK     "JOIN_OK"
#define CMD_MSG         "MSG"
#define CMD_LEAVE       "LEAVE"
#define CMD_LEAVE_OK    "LEAVE_OK"

typedef struct {
    char cmd[20];
    char arg1[50];
    char arg2[50];
    char data[256];
} Packet;

// Core Functions
void auto_load_driver();
void crypt_msg(char *msg, int mode);
int  connect_to_server(const char *ip);
void send_packet(int sock, const char *cmd, const char *arg1, const char *arg2, const char *data);

#endif
