#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <termios.h>

#define PORT 9000
#define AES_ENCRYPT 0
#define AES_DECRYPT 1

typedef struct {
    char cmd[20];
    char arg1[50];
    char arg2[50];
    char data[256];
} Packet;

int sock;
int is_logged_in = 0;
char my_username[50];
int current_room = -1;

void get_password(const char *prompt, char *buffer) {
    struct termios oldt, newt;
    int i = 0;
    char c = 0;

    printf("%s", prompt);
    fflush(stdout);

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // Disable canonical mode (buffered i/o) and local echo
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (1) {
        c = getchar();
        
        if (c == '\n' || c == '\r') {
            buffer[i] = '\0';
            break;
        } else if (c == 127 || c == 8) { // Backspace
            if (i > 0) {
                i--;
                printf("\b \b");
                fflush(stdout);
            }
        } else {
            buffer[i++] = c;
            printf("*");
            fflush(stdout);
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n");
}

void crypt_msg(char *msg, int mode)
{
    int fd = open("/dev/aes_driver", O_RDWR);
    if(fd < 0) {
        perror("Failed to open AES driver");
        return;
    }

    ioctl(fd, mode);
    write(fd, msg, 256);
    read(fd, msg, 256);
    close(fd);
}

void *receiver(void *arg)
{
    Packet pkt;
    while(1)
    {
        memset(&pkt, 0, sizeof(Packet));
        int bytes_read = read(sock, &pkt, sizeof(Packet));
        
        if(bytes_read <= 0) {
            printf("\nDisconnected from server.\n");
            exit(1);
        }

        if(strcmp(pkt.cmd, "REGISTER_OK") == 0) {
            printf("\n> Registration successful! You can now login.\n> ");
            fflush(stdout);
        }
        else if(strcmp(pkt.cmd, "REGISTER_ERR") == 0) {
            printf("\n> Registration failed: %s\n> ", pkt.arg1);
            fflush(stdout);
        }
        else if(strcmp(pkt.cmd, "LOGIN_OK") == 0) {
            is_logged_in = 1;
            printf("\n> Login successful! Welcome, %s.\n> ", my_username);
            fflush(stdout);
        }
        else if(strcmp(pkt.cmd, "LOGIN_ERR") == 0) {
            printf("\n> Login failed: %s\n> ", pkt.arg1);
            fflush(stdout);
        }
        else if(strcmp(pkt.cmd, "CREATE_OK") == 0) {
            current_room = 1; // logical flag because auto-join
            printf("\n> Room created and joined successfully! Type your message and press ENTER to send.\n> ");
            fflush(stdout);
        }
        else if(strcmp(pkt.cmd, "CREATE_ERR") == 0) {
            printf("\n> Failed to create room (limit reached or error).\n> ");
            fflush(stdout);
        }
        else if(strcmp(pkt.cmd, "JOIN_OK") == 0) {
            current_room = 1; // logical flag
            printf("\n> Joined room successfully! Type your message and press ENTER to send.\n> ");
            fflush(stdout);
        }
        else if(strcmp(pkt.cmd, "JOIN_ERR") == 0) {
            printf("\n> Failed to join room (not found).\n> ");
            fflush(stdout);
        }
        else if(strcmp(pkt.cmd, "MSG") == 0) {
            // End-to-end decryption
            crypt_msg(pkt.data, AES_DECRYPT);
            
            // clear the current line if there is ongoing input or `> `
            if(strcmp(pkt.arg1, my_username) == 0) {
                printf("\33[2K\r[me]: %s\n> ", pkt.data);
            } else {
                printf("\33[2K\r[%s]: %s\n> ", pkt.arg1, pkt.data);
            }
            fflush(stdout);
        }
    }
    return NULL;
}

void print_menu() {
    if(current_room != -1) return; // avoid printing menu when in a chat room

    printf("\n--- CHAT AES T3 ---\n");
    if(!is_logged_in) {
        printf("1. Register\n");
        printf("2. Login\n");
        printf("\nChoose an option (1/2): ");
    } else if(current_room == -1) {
        printf("1. Create Room\n");
        printf("2. Join Room\n");
        printf("\nChoose an option (1/2): ");
    }
    fflush(stdout);
}

int main()
{
    struct sockaddr_in server;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Connection failed");
        return 1;
    }

    pthread_t tid;
    pthread_create(&tid, NULL, receiver, NULL);

    char input[256];
    char cmd[50], arg1[50], arg2[50];
    Packet pkt;

    while(1)
    {
        print_menu();
        if(fgets(input, 256, stdin) == NULL) continue;
        
        // If in a chat room, erase the line the user just typed so it doesn't leave duplicates
        if (current_room != -1) {
            printf("\033[1A\033[2K\r");
            fflush(stdout);
        }

        // Remove trailing newline
        input[strcspn(input, "\n")] = 0;
        if(strlen(input) == 0) continue;

        memset(&pkt, 0, sizeof(Packet));

        if (!is_logged_in) {
            if(strcmp(input, "1") == 0) {
                char confirm_pw[50];
                printf("Enter new username: ");
                scanf("%s", arg1);
                getchar(); // consume newline
                
                while(1) {
                    get_password("Enter new password: ", arg2);
                    get_password("Confirm new password: ", confirm_pw);
                    
                    if (strcmp(arg2, confirm_pw) != 0) {
                        printf("Passwords do not match. Please try again.\n");
                    } else {
                        break;
                    }
                }

                strcpy(pkt.cmd, "REGISTER");
                strcpy(pkt.arg1, arg1);
                strcpy(pkt.arg2, arg2);
                send(sock, &pkt, sizeof(Packet), 0);
            } 
            else if(strcmp(input, "2") == 0) {
                printf("Enter username: ");
                scanf("%s", arg1);
                getchar(); // consume newline
                get_password("Enter password: ", arg2);
                
                strcpy(pkt.cmd, "LOGIN");
                strcpy(pkt.arg1, arg1);
                strcpy(pkt.arg2, arg2);
                strcpy(my_username, arg1);
                send(sock, &pkt, sizeof(Packet), 0);
            } else {
                printf("Invalid option.\n");
            }
        } 
        else if(current_room == -1) {
            if(strcmp(input, "1") == 0) {
                printf("Enter room name to create: ");
                scanf("%s", arg1);
                getchar(); // consume newline

                strcpy(pkt.cmd, "CREATE");
                strcpy(pkt.arg1, arg1);
                send(sock, &pkt, sizeof(Packet), 0);
            } 
            else if(strcmp(input, "2") == 0) {
                printf("Enter room name to join: ");
                scanf("%s", arg1);
                getchar(); // consume newline

                strcpy(pkt.cmd, "JOIN");
                strcpy(pkt.arg1, arg1);
                send(sock, &pkt, sizeof(Packet), 0);
            } else {
                printf("Invalid option.\n");
            }
        }
        else {
            strcpy(pkt.cmd, "MSG");
            strcpy(pkt.arg1, my_username);
            
            memset(pkt.data, 0, 256);
            strncpy(pkt.data, input, 255);
            
            crypt_msg(pkt.data, AES_ENCRYPT);

            send(sock, &pkt, sizeof(Packet), 0);
        }
        
        // slight delay to let the receive thread parse responses before re-printing menu
        usleep(100000); 
    }

    return 0;
}