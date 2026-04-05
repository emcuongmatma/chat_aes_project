#include "client_shared.h"
#include <termios.h>

int sock;
int is_logged_in = 0;
char my_username[50];
int current_room = -1;

void print_menu();

// --- UTILITIES ---

void get_password(const char *prompt, char *buffer) {
    struct termios oldt, newt;
    int i = 0; char c;
    printf("%s", prompt); fflush(stdout);
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt; newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    while (1) {
        c = getchar();
        if (c == '\n' || c == '\r') { buffer[i] = '\0'; break; }
        else if (c == 127 || c == 8) { if (i > 0) { i--; printf("\b \b"); fflush(stdout); } }
        else { buffer[i++] = c; printf("*"); fflush(stdout); }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n");
}

void print_menu() {
    if(current_room != -1) return;
    printf("\n--- CHAT AES T3 ---\n");
    if(!is_logged_in) printf("1. Register\n2. Login\nChoose: ");
    else printf("1. Create Room\n2. Join Room\nChoose: ");
    fflush(stdout);
}

// --- INPUT HANDLERS ---

void handle_register_input() {
    char user[50], pass[50], confirm[50];
    printf("Username: "); scanf("%s", user);
    while(1) {
        get_password("Password: ", pass);
        get_password("Confirm: ", confirm);
        if (strcmp(pass, confirm) == 0) break;
        printf("Mismatch, retry.\n");
    }
    send_packet(sock, CMD_REGISTER, user, pass, NULL);
}

void handle_login_input() {
    char user[50], pass[50];
    printf("Username: "); scanf("%s", user);
    get_password("Password: ", pass);
    strcpy(my_username, user);
    send_packet(sock, CMD_LOGIN, user, pass, NULL);
}

void handle_create_room_input() {
    char room[50];
    printf("Room name: "); scanf("%s", room);
    send_packet(sock, CMD_CREATE, room, NULL, NULL);
}

void handle_join_room_input() {
    char room[50];
    printf("Room name: "); scanf("%s", room);
    send_packet(sock, CMD_JOIN, room, NULL, NULL);
}

void handle_chat_input(char *input) {
    if (strcmp(input, "/leave") == 0 || strcmp(input, "/quit") == 0) {
        send_packet(sock, CMD_LEAVE, NULL, NULL, NULL);
        return;
    }
    char encrypted[256]; memset(encrypted, 0, 256);
    strncpy(encrypted, input, 255);
    crypt_msg(encrypted, AES_ENCRYPT);
    send_packet(sock, CMD_MSG, my_username, NULL, encrypted);
}

// --- NETWORK HANDLER ---

void process_response(Packet *pkt) {
    if(strcmp(pkt->cmd, CMD_REGISTER_OK) == 0) printf("\n> Reg OK! Login now.\n> ");
    else if(strcmp(pkt->cmd, CMD_LOGIN_OK) == 0) {
        is_logged_in = 1;
        printf("\n> Welcome, %s!\n> ", my_username);
    }
    else if(strcmp(pkt->cmd, CMD_CREATE_OK) == 0 || strcmp(pkt->cmd, CMD_JOIN_OK) == 0) {
        current_room = 1;
        printf("\n> Room entered. /leave to exit.\n> ");
    }
    else if(strcmp(pkt->cmd, CMD_LEAVE_OK) == 0) {
        current_room = -1;
        printf("\n> Left room.\n");
    }
    else if(strcmp(pkt->cmd, CMD_MSG) == 0) {
        crypt_msg(pkt->data, AES_DECRYPT);
        char *sender = strcmp(pkt->arg1, my_username) == 0 ? "me" : pkt->arg1;
        printf("\33[2K\r[%s]: %s\n> ", sender, pkt->data);
    }
    else if(strstr(pkt->cmd, "_ERR")) printf("\n> Error: %s\n> ", pkt->arg1);
    fflush(stdout);
}

void *receiver(void *arg) {
    Packet pkt;
    while(read(sock, &pkt, sizeof(Packet)) > 0) process_response(&pkt);
    printf("\nDisconnected.\n"); exit(1);
    return NULL;
}

// --- MAIN ---

int main(int argc, char *argv[]) {
    char ip[50] = "127.0.0.1";
    if (argc > 1) strncpy(ip, argv[1], 49);
    
    auto_load_driver();
    sock = connect_to_server(ip);
    if (sock < 0) { perror("Conn failed"); return 1; }

    pthread_t tid; pthread_create(&tid, NULL, receiver, NULL);
    char input[256];
    while(1) {
        print_menu();
        if(fgets(input, 256, stdin) == NULL) continue;
        if(current_room != -1) printf("\033[1A\033[2K\r");
        input[strcspn(input, "\n")] = 0;
        if(strlen(input) == 0) continue;

        if (!is_logged_in) {
            if(strcmp(input, "1") == 0) handle_register_input();
            else if(strcmp(input, "2") == 0) handle_login_input();
        } 
        else if(current_room == -1) {
            if(strcmp(input, "1") == 0) handle_create_room_input();
            else if(strcmp(input, "2") == 0) handle_join_room_input();
        }
        else handle_chat_input(input);
        usleep(100000); 
    }
    return 0;
}