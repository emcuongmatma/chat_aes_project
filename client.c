#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define PORT 8080

int main()
{
    int sock;
    struct sockaddr_in serv_addr;

    char message[256];
    char response[256];

    sock = socket(AF_INET, SOCK_STREAM, 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    printf("Enter message: ");
    fgets(message, 256, stdin);

    send(sock, message, strlen(message), 0);

    read(sock, response, 256);

    printf("Encrypted message from server: %s\n", response);

    close(sock);

    return 0;
}