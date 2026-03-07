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

//Packet Model
typedef struct {
    char cmd[20]; //lệnh thực thi
    char arg1[50]; //tài khoản
    char arg2[50]; //mật khẩu
    char data[256]; //nội dung gửi đi
} Packet;

//Client Model
typedef struct {
    int socket;
    char username[50];
    int room;
    int is_logged_in;
} Client;

//Room chat Model
typedef struct {
    char name[50];
    int members[MAX_CLIENTS];
    int count;
} Room;

//Mảng lưu trữ client
Client clients[MAX_CLIENTS];
//Mảng lưu trữ phòng chat
Room rooms[MAX_ROOMS];

int room_count = 0;

//Mutex để tránh xung đột khi truy cập tài nguyên (đăng ký tài khoản và đăng ký phòng chat)
pthread_mutex_t auth_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_lock = PTHREAD_MUTEX_INITIALIZER;

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
    //cấp phát bộ nhớ cho response
    memset(&response, 0, sizeof(Packet));
    //khóa thread đăng ký tài khoản
    pthread_mutex_lock(&auth_lock);
    FILE *f = fopen("users.txt", "a+");
    //đưa con trỏ về đầu (để check tồn tại)
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
        fprintf(f, "%s %s\n", pkt->arg1, pkt->arg2);
        strcpy(response.cmd, "REGISTER_OK");
    } else {
        strcpy(response.cmd, "REGISTER_ERR");
        strcpy(response.arg1, "Username exists");
    }
    fclose(f);
    pthread_mutex_unlock(&auth_lock);
    //gửi kết quả về client
    send(sock, &response, sizeof(Packet), 0);
}

//Hàm xử lý đăng nhập tài khoản
void handle_login(int sock, Packet *pkt, char *username, int *is_logged_in) {
    Packet response;
    memset(&response, 0, sizeof(Packet));
    
    pthread_mutex_lock(&auth_lock);
    FILE *f = fopen("users.txt", "r");
    int valid = 0;
    if(f) {
        char u[50], p[50];
        while(fscanf(f, "%s %s", u, p) != EOF) {
            if(strcmp(u, pkt->arg1) == 0 && strcmp(p, pkt->arg2) == 0) {
                valid = 1;
                break;
            }
        }
        fclose(f);
    }
    pthread_mutex_unlock(&auth_lock);

    if(valid) {
        strcpy(username, pkt->arg1);
        //lưu trạng thái đã đăng nhập (để check auth khi tạo phòng, gửi tin nhắn,.. ở phía server)
        *is_logged_in = 1;
        strcpy(response.cmd, "LOGIN_OK");
    } else {
        strcpy(response.cmd, "LOGIN_ERR");
        strcpy(response.arg1, "Invalid credentials");
    }
    send(sock, &response, sizeof(Packet), 0);
}

void handle_create(int sock, Packet *pkt, int is_logged_in, int *room) {
    if(!is_logged_in) return;
    Packet response;
    memset(&response, 0, sizeof(Packet));
    
    pthread_mutex_lock(&rooms_lock);
    int exists = 0;
    //check tồn tại
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
        //tạo room mới
        strcpy(rooms[room_count].name, pkt->arg1);
        rooms[room_count].count = 0;
        int new_room_idx = room_count;
        room_count++;
        
        // Trả về room id cho thread của client (tự động vào phòng)
        *room = new_room_idx;
        rooms[new_room_idx].members[rooms[new_room_idx].count++] = sock;
        
        // Tạo file lưu trữ lịch sử chat
        char hist_file[100];
        snprintf(hist_file, sizeof(hist_file), "%s.hist", rooms[new_room_idx].name);
        FILE *hf = fopen(hist_file, "ab");
        if (hf) fclose(hf);

        strcpy(response.cmd, "CREATE_OK");
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
        
        // Send chat history
        char hist_file[100];
        snprintf(hist_file, sizeof(hist_file), "%s.hist", rooms[*room].name);
        FILE *hf = fopen(hist_file, "rb");
        if (hf) {
            Packet hist_pkt;
            //đọc từng khối packet của file history rồi gửi vêf cho client mới đăng nhập
            while (fread(&hist_pkt, sizeof(Packet), 1, hf) == 1) {
                send(sock, &hist_pkt, sizeof(Packet), 0);
            }
            fclose(hf);
        }
    } else {
        strcpy(response.cmd, "JOIN_ERR");
        send(sock, &response, sizeof(Packet), 0);
    }
}

void remove_client_from_room(int sock, int *room) {
    if (*room == -1) return;
    
    pthread_mutex_lock(&rooms_lock);
    for(int i = 0; i < rooms[*room].count; i++) {
        if(rooms[*room].members[i] == sock) {
            rooms[*room].members[i] = rooms[*room].members[rooms[*room].count - 1];
            rooms[*room].count--;
            break;
        }
    }
    if (rooms[*room].count == 0) {
        //xóa file chat khi không còn ai trong phòng
        char hist_file[100];
        snprintf(hist_file, sizeof(hist_file), "%s.hist", rooms[*room].name);
        //xóa room trong mảng
        rooms[*room] = rooms[room_count - 1];
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
    //check trạng thái đăng nhập và phòng hiện tại của client
    if(!is_logged_in || room == -1) return;
    
    // Re-broadcast the packet to all members in the room
    broadcast_packet(room, pkt);

    // Save to history file
    pthread_mutex_lock(&rooms_lock);
    char hist_file[100];
    snprintf(hist_file, sizeof(hist_file), "%s.hist", rooms[room].name);
    FILE *hf = fopen(hist_file, "ab");
    if (hf) {
        fwrite(pkt, sizeof(Packet), 1, hf);
        fclose(hf);
    }
    pthread_mutex_unlock(&rooms_lock);
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
    FILE *f = fopen("users.txt", "a+");
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