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

void print_menu();

/* =========================================================
 * DRIVER MANAGEMENT
 * ========================================================= */

void auto_load_driver() {
    int fd = open("/dev/aes_driver", O_RDWR);
    if (fd != -1) {
        close(fd);
        return; // Driver đã được nạp
    }

    printf("[Hệ thống] Không tìm thấy driver AES. Đang thử nạp...\n");

    // Đường dẫn tương đối tới file driver .ko (giả định chạy từ thư mục client/)
    const char *driver_path = "../driver/aes_driver.ko";

    // 1. Thử nạp module vào kernel
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "sudo insmod %s 2>/dev/null", driver_path);
    system(cmd);

    // 2. Lấy số Major được cấp phát động
    FILE *fp = popen("awk '$2==\"aes_driver\" {print $1}' /proc/devices", "r");
    if (fp) {
        char major_str[10];
        if (fgets(major_str, sizeof(major_str), fp)) {
            int major = atoi(major_str);
            // 3. Tạo device node và cấp quyền
            snprintf(cmd, sizeof(cmd), 
                     "sudo rm -f /dev/aes_driver && "
                     "sudo mknod /dev/aes_driver c %d 0 && "
                     "sudo chmod 666 /dev/aes_driver", major);
            if (system(cmd) == 0) {
                printf("[Hệ thống] Nạp driver AES thành công (Major: %d).\n", major);
            }
        } else {
            fprintf(stderr, "[Lỗi] Không tìm thấy 'aes_driver' trong /proc/devices. Driver đã được biên dịch chưa?\n");
        }
        pclose(fp);
    }
}

/* =========================================================
 * UTILITY FUNCTIONS
 * ========================================================= */

void get_password(const char *prompt, char *buffer) {
    struct termios oldt, newt;
    int i = 0;
    char c = 0;

    printf("%s", prompt);
    fflush(stdout);

    //tắt echo và canonical mode để không hiển thị ký tự khi gõ
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    //đọc từng ký tự vã vẽ trên màn hình với password ẩn
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

    //gọi lệnh điều khiển IO để mã hóa/ giải mã
    //mode AES_ENCRYPT để mã hóa
    //mode AES_DECRYPT để giải mã
    ioctl(fd, mode);
    write(fd, msg, 256);
    read(fd, msg, 256);
    close(fd);
}

//hàm in menu chính
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

/* =========================================================
 * INPUT HANDLERS
 * ========================================================= */

void handle_register_input() {
    Packet pkt;
    char arg1[50], arg2[50], confirm_pw[50];

    memset(&pkt, 0, sizeof(Packet));
    printf("Enter new username: ");
    scanf("%s", arg1);
    getchar(); // consume newline
    
    //input and check valid password
    while(1) {
        get_password("Enter new password: ", arg2);
        get_password("Confirm new password: ", confirm_pw);
        
        if (strcmp(arg2, confirm_pw) != 0) {
            printf("Passwords do not match. Please try again.\n");
        } else {
            break;
        }
    }

    //tạo và gửi request cho servẻr qua socket
    strcpy(pkt.cmd, "REGISTER");
    strcpy(pkt.arg1, arg1);
    strcpy(pkt.arg2, arg2);
    send(sock, &pkt, sizeof(Packet), 0);
}

void handle_login_input() {
    Packet pkt;
    char arg1[50], arg2[50];

    memset(&pkt, 0, sizeof(Packet));
    printf("Enter username: ");
    scanf("%s", arg1);
    getchar(); // consume newline
    get_password("Enter password: ", arg2);
    
    //tạo và gửi request cho servẻr qua socket
    strcpy(pkt.cmd, "LOGIN");
    strcpy(pkt.arg1, arg1);
    strcpy(pkt.arg2, arg2);
    strcpy(my_username, arg1); // Store locally for session
    send(sock, &pkt, sizeof(Packet), 0);
}

void handle_create_room_input() {
    Packet pkt;
    char arg1[50];

    memset(&pkt, 0, sizeof(Packet));
    printf("Enter room name to create: ");
    scanf("%s", arg1);
    getchar(); // consume newline

    strcpy(pkt.cmd, "CREATE");
    strcpy(pkt.arg1, arg1);
    send(sock, &pkt, sizeof(Packet), 0);
}

void handle_join_room_input() {
    Packet pkt;
    char arg1[50];

    memset(&pkt, 0, sizeof(Packet));
    printf("Enter room name to join: ");
    scanf("%s", arg1);
    getchar(); // consume newline

    strcpy(pkt.cmd, "JOIN");
    strcpy(pkt.arg1, arg1);
    send(sock, &pkt, sizeof(Packet), 0);
}

//hàm xử lý người dùng nhập vào trong khung chat
void handle_chat_input(char *input) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));

    if (strcmp(input, "/leave") == 0 || strcmp(input, "/quit") == 0) {
        strcpy(pkt.cmd, "LEAVE");
        send(sock, &pkt, sizeof(Packet), 0);
        return;
    }

    strcpy(pkt.cmd, "MSG");
    strcpy(pkt.arg1, my_username);
    
    strncpy(pkt.data, input, 255);
    crypt_msg(pkt.data, AES_ENCRYPT);

    send(sock, &pkt, sizeof(Packet), 0);
}

/* =========================================================
 * SERVER RESPONSE HANDLER
 * ========================================================= */

void process_server_response(Packet *pkt) {
    if(strcmp(pkt->cmd, "REGISTER_OK") == 0) {
        printf("\n> Registration successful! You can now login.\n> ");
        fflush(stdout);
    }
    else if(strcmp(pkt->cmd, "REGISTER_ERR") == 0) {
        printf("\n> Registration failed: %s\n> ", pkt->arg1);
        fflush(stdout);
    }
    else if(strcmp(pkt->cmd, "LOGIN_OK") == 0) {
        is_logged_in = 1;
        printf("\n> Login successful! Welcome, %s.\n> ", my_username);
        fflush(stdout);
    }
    else if(strcmp(pkt->cmd, "LOGIN_ERR") == 0) {
        printf("\n> Login failed: %s\n> ", pkt->arg1);
        fflush(stdout);
    }
    else if(strcmp(pkt->cmd, "CREATE_OK") == 0) {
        current_room = 1; // logical flag
        printf("\n> Room created and joined successfully! Type your message and press ENTER to send. Type /leave to exit.\n> ");
        fflush(stdout);
    }
    else if(strcmp(pkt->cmd, "CREATE_ERR") == 0) {
        printf("\n> Failed to create room: %s\n> ", pkt->arg1);
        fflush(stdout);
    }
    else if(strcmp(pkt->cmd, "JOIN_OK") == 0) {
        current_room = 1; // logical flag
        printf("\n> Joined room successfully! Type your message and press ENTER to send. Type /leave to exit.\n> ");
        fflush(stdout);
    }
    else if(strcmp(pkt->cmd, "JOIN_ERR") == 0) {
        printf("\n> Failed to join room (not found).\n> ");
        fflush(stdout);
    }
    else if(strcmp(pkt->cmd, "LEAVE_OK") == 0) {
        current_room = -1; // reset room state
        printf("\n> Left the room successfully.\n");
        fflush(stdout);
    }
    else if(strcmp(pkt->cmd, "MSG") == 0) {
        crypt_msg(pkt->data, AES_DECRYPT);
        
        if(strcmp(pkt->arg1, my_username) == 0) {
            printf("\33[2K\r[me]: %s\n> ", pkt->data);
            //\33[2K \r là lệnh xóa dòng và đưa con trỏ về đầu dòng
        } else {
            printf("\33[2K\r[%s]: %s\n> ", pkt->arg1, pkt->data);
        }
        fflush(stdout);
    }
}

//hàm nhận response từ server
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

        process_server_response(&pkt);
    }
    return NULL;
}

/* =========================================================
 * MAIN PROGRAM
 * ========================================================= */

int main(int argc, char *argv[])
{
    char server_ip[50] = "127.0.0.1";
    if (argc > 1) {
        strncpy(server_ip, argv[1], 49);
    }

    // Tự động nạp driver nếu chưa có
    auto_load_driver();

    //khởi tạo socket client
    struct sockaddr_in server;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, server_ip, &server.sin_addr) <= 0) {
        printf("Invalid IP address format: %s. Using 127.0.0.1 instead.\n", server_ip);
        inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);
    } else {
        printf("Connecting to server at %s:%d...\n", server_ip, PORT);
    }

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Connection failed");
        return 1;
    }

    pthread_t tid;
    pthread_create(&tid, NULL, receiver, NULL);

    char input[256];

    while(1)
    {
        print_menu();
        if(fgets(input, 256, stdin) == NULL) continue;
        
        // Emulate clean chat flow by deleting typed line.
        // Tránh trùng lặp tin nhắn vừa gõ và tin nhắn trả về từ server
        if (current_room != -1) {
            printf("\033[1A\033[2K\r");
            fflush(stdout);
        }

        input[strcspn(input, "\n")] = 0;
        //xóa ký tự xuống dòng
        if(strlen(input) == 0) continue;
        //input trống => chạy lại từ đầu

        if (!is_logged_in) {
            //xử lý tùy chọn đăng nhập/ đăng ký
            if(strcmp(input, "1") == 0) {
                handle_register_input();
            } else if(strcmp(input, "2") == 0) {
                handle_login_input();
            } else {
                printf("Invalid option.\n");
            }
        } 
        else if(current_room == -1) {
            //xử lý tùy chọn tạo phòng/ tham gia phòng
            if(strcmp(input, "1") == 0) {
                handle_create_room_input();
            } else if(strcmp(input, "2") == 0) {
                handle_join_room_input();
            } else {
                printf("Invalid option.\n");
            }
        }
        else {
            handle_chat_input(input);
        }
        
        //delay 0.1s để tránh gửi quá nhanh
        usleep(100000); 
    }

    return 0;
}