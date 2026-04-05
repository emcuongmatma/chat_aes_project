#include "client_shared.h"

void auto_load_driver() {
    int fd = open("/dev/aes_driver", O_RDWR);
    if (fd != -1) {
        close(fd);
        return; 
    }

    printf("[Hệ thống] Không tìm thấy driver AES. Đang thử nạp...\n");

    const char *driver_path = "../driver/aes_driver.ko";

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "sudo insmod %s 2>/dev/null", driver_path);
    system(cmd);

    FILE *fp = popen("awk '$2==\"aes_driver\" {print $1}' /proc/devices", "r");
    if (fp) {
        char major_str[10];
        if (fgets(major_str, sizeof(major_str), fp)) {
            int major = atoi(major_str);
            snprintf(cmd, sizeof(cmd), 
                     "sudo rm -f /dev/aes_driver && "
                     "sudo mknod /dev/aes_driver c %d 0 && "
                     "sudo chmod 666 /dev/aes_driver", major);
            system(cmd);
        }
        pclose(fp);
    }
}

void crypt_msg(char *msg, int mode) {
    int fd = open("/dev/aes_driver", O_RDWR);
    if(fd < 0) return;
    ioctl(fd, mode);
    write(fd, msg, 256);
    read(fd, msg, 256);
    close(fd);
}

int connect_to_server(const char *ip) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, ip, &server.sin_addr) <= 0) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}
