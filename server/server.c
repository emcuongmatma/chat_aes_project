#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define PORT 9000
#define MAX_CLIENTS 50
#define MAX_ROOMS 10
#define HASH_SIZE 17  // 16 chars + null terminator

// ===============================================
// MODELS & STRUCTURES
// ===============================================

// Packet Model - Cấu trúc dữ liệu gửi qua lại giữa Client và Server
typedef struct {
    char cmd[20];   // Lệnh thực thi (LOGIN, REGISTER, MSG, ...)
    char arg1[50];  // Tham số 1 (thường là username hoặc tên phòng)
    char arg2[50];  // Tham số 2 (thường là password)
    char data[256]; // Nội dung truyền tải (tin nhắn mã hóa)
} Packet;

// Client Model - Thông tin quản lý một kết nối người dùng
typedef struct {
    int socket;
    char username[50];
    int room;       // ID phòng hiện tại (-1 nếu không ở trong phòng)
    int is_logged_in;
} Client;

// Room chat Model - Thông tin quản lý một phòng chat
typedef struct {
    char name[50];
    int members[MAX_CLIENTS];
    int count;
} Room;

// ===============================================
// GLOBAL VARIABLES
// ===============================================

Room rooms[MAX_ROOMS];
int room_count = 0;

// Mutex bảo vệ dữ liệu dùng chung
pthread_mutex_t auth_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_lock = PTHREAD_MUTEX_INITIALIZER;

// ===============================================
// SECURITY FUNCTIONS
// ===============================================

/**
 * Hàm băm mật khẩu đơn giản (djb2 hash)
 * Mục đích: Không lưu mật khẩu text rõ vào database để bảo đảm an toàn.
 */
void hash_password(const char *pass, char *out) {
    unsigned long hash = 5381;
    int c;
    while ((c = *pass++))
        hash = ((hash << 5) + hash) + c; 
    sprintf(out, "%016lx", hash);
}

// ===============================================
// CORE FUNCTIONS
// ===============================================

//Hàm gửi packet đến tất cả các thành viên trong phòng
void broadcast_packet(int room, Packet *pkt)
{
    //khóa thread broadcast (tránh bị gọi trùng cùng 1 thời điểm)
    pthread_mutex_lock(&rooms_lock);
    for(int i = 0; i < rooms[room].count; i++)
    {
        int member_sock = rooms[room].members[i];
        send(member_sock, pkt, sizeof(Packet), 0);
    }
    //mở khóa thread broadcast
    pthread_mutex_unlock(&rooms_lock);
}

//Hàm xử lý đăng ký tài khoản
void handle_register(int sock, Packet *pkt) {
    Packet response;
    memset(&response, 0, sizeof(Packet));
    
    char hashed_pass[HASH_SIZE];
    hash_password(pkt->arg2, hashed_pass);

    pthread_mutex_lock(&auth_lock);
    FILE *f = fopen("users.db", "a+");
    if (!f) {
        pthread_mutex_unlock(&auth_lock);
        return;
    }

    fseek(f, 0, SEEK_SET);
    char u[50], p[50];
    int exists = 0;
    while(fscanf(f, "%s %s", u, p) != EOF) {
        if(strcmp(u, pkt->arg1) == 0) {
            exists = 1;
            break;
        }
    }

    if(!exists) {
        fprintf(f, "%s %s\n", pkt->arg1, hashed_pass);
        strcpy(response.cmd, "REGISTER_OK");
        printf("[Log] Registered new user: %s\n", pkt->arg1);
    } else {
        strcpy(response.cmd, "REGISTER_ERR");
        strcpy(response.arg1, "Username exists");
    }
    
    fclose(f);
    pthread_mutex_unlock(&auth_lock);
    send(sock, &response, sizeof(Packet), 0);
}

//Hàm xử lý đăng nhập tài khoản
void handle_login(int sock, Packet *pkt, char *username, int *is_logged_in) {
    Packet response;
    memset(&response, 0, sizeof(Packet));
    
    char hashed_pass[HASH_SIZE];
    hash_password(pkt->arg2, hashed_pass);

    pthread_mutex_lock(&auth_lock);
    FILE *f = fopen("users.db", "r");
    int valid = 0;
    if(f) {
        char u[50], p[50];
        while(fscanf(f, "%s %s", u, p) != EOF) {
            if(strcmp(u, pkt->arg1) == 0 && strcmp(p, hashed_pass) == 0) {
                valid = 1;
                break;
            }
        }
        fclose(f);
    }
    pthread_mutex_unlock(&auth_lock);

    if(valid) {
        strcpy(username, pkt->arg1);
        *is_logged_in = 1;
        strcpy(response.cmd, "LOGIN_OK");
        printf("[Log] User logged in: %s\n", username);
    } else {
        strcpy(response.cmd, "LOGIN_ERR");
        strcpy(response.arg1, "Invalid credentials");
    }
    send(sock, &response, sizeof(Packet), 0);
}

// ===============================================
// ROOM & MESSAGE HANDLERS
// ===============================================

void handle_create(int sock, Packet *pkt, int is_logged_in, int *room) {
    if(!is_logged_in) return;
    Packet response;
    memset(&response, 0, sizeof(Packet));
    
    pthread_mutex_lock(&rooms_lock);
    int exists = 0;
    for(int i = 0; i < room_count; i++) {
        if(strcmp(rooms[i].name, pkt->arg1) == 0) {
            exists = 1;
            break;
        }
    }
    
    if (exists) {
        strcpy(response.cmd, "CREATE_ERR");
        strcpy(response.arg1, "Room already exists");
    } else if (room_count < MAX_ROOMS) {
        // Khởi tạo phòng mới
        strcpy(rooms[room_count].name, pkt->arg1);
        rooms[room_count].count = 0;
        int new_room_idx = room_count;
        room_count++;
        
        // Tự động cho người tạo vào phòng
        *room = new_room_idx;
        rooms[new_room_idx].members[rooms[new_room_idx].count++] = sock;
        
        // Tạo file lưu trữ lịch sử chat rỗng
        char hist_file[100];
        snprintf(hist_file, sizeof(hist_file), "%s.hist", rooms[new_room_idx].name);
        FILE *hf = fopen(hist_file, "ab");
        if (hf) fclose(hf);

        strcpy(response.cmd, "CREATE_OK");
        printf("[Log] Room created: %s by socket %d\n", pkt->arg1, sock);
    } else {
        strcpy(response.cmd, "CREATE_ERR");
        strcpy(response.arg1, "Max rooms reached");
    }
    pthread_mutex_unlock(&rooms_lock);
    send(sock, &response, sizeof(Packet), 0);
}

void handle_join(int sock, Packet *pkt, int is_logged_in, int *room) {
    if(!is_logged_in) return;
    Packet response;
    memset(&response, 0, sizeof(Packet));
    
    pthread_mutex_lock(&rooms_lock);
    int found = 0;
    for(int i = 0; i < room_count; i++) {
        if(strcmp(rooms[i].name, pkt->arg1) == 0) {
            *room = i;
            rooms[i].members[rooms[i].count++] = sock;
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&rooms_lock);
    
    if(found) {
        strcpy(response.cmd, "JOIN_OK");
        send(sock, &response, sizeof(Packet), 0);
        printf("[Log] Socket %d joined room: %s\n", sock, rooms[*room].name);
        
        // Gửi lại lịch sử chat cho người mới vào
        char hist_file[100];
        snprintf(hist_file, sizeof(hist_file), "%s.hist", rooms[*room].name);
        FILE *hf = fopen(hist_file, "rb");
        if (hf) {
            Packet hist_pkt;
            while (fread(&hist_pkt, sizeof(Packet), 1, hf) == 1) {
                send(sock, &hist_pkt, sizeof(Packet), 0);
            }
            fclose(hf);
        }
    } else {
        strcpy(response.cmd, "JOIN_ERR");
        strcpy(response.arg1, "Room not found or empty");
        send(sock, &response, sizeof(Packet), 0);
    }
}

void remove_client_from_room(int sock, int *room) {
    if (*room == -1) return;
    
    pthread_mutex_lock(&rooms_lock);
    int r_idx = *room;
    for(int i = 0; i < rooms[r_idx].count; i++) {
        if(rooms[r_idx].members[i] == sock) {
            rooms[r_idx].members[i] = rooms[r_idx].members[rooms[r_idx].count - 1];
            rooms[r_idx].count--;
            break;
        }
    }

    // Nếu phòng không còn ai, xóa phòng và lịch sử
    if (rooms[r_idx].count == 0) {
        char hist_file[100];
        snprintf(hist_file, sizeof(hist_file), "%s.hist", rooms[r_idx].name);
        printf("[Log] Room %s is empty, deleting...\n", rooms[r_idx].name);
        
        rooms[r_idx] = rooms[room_count - 1];
        room_count--;
        remove(hist_file);
    }
    pthread_mutex_unlock(&rooms_lock);
    *room = -1;
}

void handle_leave(int sock, int is_logged_in, int *room) {
    if(!is_logged_in || *room == -1) return;
    remove_client_from_room(sock, room);
    
    Packet response;
    memset(&response, 0, sizeof(Packet));
    strcpy(response.cmd, "LEAVE_OK");
    send(sock, &response, sizeof(Packet), 0);
}

void handle_msg(int is_logged_in, int room, Packet *pkt) {
    if(!is_logged_in || room == -1) return;
    
    // Chuyển tiếp tin nhắn đến toàn bộ thành viên trong phòng
    broadcast_packet(room, pkt);

    // Lưu vào lịch sử chat
    pthread_mutex_lock(&rooms_lock);
    char hist_file[100];
    snprintf(hist_file, sizeof(hist_file), "%s.hist", rooms[room].name);
    FILE *hf = fopen(hist_file, "ab");
    if (hf) {
        fwrite(pkt, sizeof(Packet), 1, hf);
        fclose(hf);
    }
    pthread_mutex_unlock(&rooms_lock);

    // Log thông tin mã hóa ra console server
    printf("[Msg] %s -> %s: ", pkt->arg1, rooms[room].name);
    for(int i = 0; i < 16; i++) printf("%02x ", (unsigned char)pkt->data[i]);
    printf("...\n");
}

//tạo thread cho từng client
void *client_thread(void *arg)
{
    int sock = *(int*)arg;
    Packet pkt;
    int room = -1;
    char username[50] = "";
    int is_logged_in = 0;

    while(1)
    {
        memset(&pkt, 0, sizeof(Packet));
        int bytes_read = read(sock, &pkt, sizeof(Packet));
        
        if(bytes_read <= 0) {
            printf("Client disconnected\n");
            remove_client_from_room(sock, &room);
            break;
        }

        if(strcmp(pkt.cmd, "REGISTER") == 0) {
            handle_register(sock, &pkt);
        }
        else if(strcmp(pkt.cmd, "LOGIN") == 0) {
            handle_login(sock, &pkt, username, &is_logged_in);
        }
        else if(strcmp(pkt.cmd, "CREATE") == 0) {
            handle_create(sock, &pkt, is_logged_in, &room);
        }
        else if(strcmp(pkt.cmd, "JOIN") == 0) {
            handle_join(sock, &pkt, is_logged_in, &room);
        }
        else if(strcmp(pkt.cmd, "LEAVE") == 0) {
            handle_leave(sock, is_logged_in, &room);
        }
        else if(strcmp(pkt.cmd, "MSG") == 0) {
            handle_msg(is_logged_in, room, &pkt);
        }
    }
    close(sock);
    return NULL;
}

int main()
{
    //khởi tạo server
    int server_fd, client_socket;
    struct sockaddr_in server, client;
    socklen_t len = sizeof(client);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    // Allow address reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        return 1;
    }
    
    listen(server_fd, 10);
    printf("Server started on port %d\n", PORT);
    //đảm bảo khởi tạo database
    FILE *f = fopen("users.db", "a+");
    if(f) fclose(f);

    while(1)
    {
        //lắng nghe kết nối mới từ client
        client_socket = accept(server_fd, (struct sockaddr*)&client, &len);
        if (client_socket < 0) continue;

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, &client_socket);
        // Detach thread to prevent memory leak
        pthread_detach(tid);
    }
    return 0;
}