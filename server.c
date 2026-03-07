#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define PORT 8080

int main()
{
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    char buffer[256];
    char encrypted[256];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    printf("Server waiting...\n");

    client_socket = accept(server_fd,
                           (struct sockaddr *)&address,
                           (socklen_t*)&addrlen);

    read(client_socket, buffer, 256);

    printf("Client message: %s\n", buffer);

    int fd = open("/dev/aes_driver", O_RDWR);

    write(fd, buffer, strlen(buffer));

    read(fd, encrypted, strlen(buffer));

    printf("Encrypted: %s\n", encrypted);

    send(client_socket, encrypted, strlen(buffer), 0);

    close(fd);
    close(client_socket);
    close(server_fd);

    return 0;
}