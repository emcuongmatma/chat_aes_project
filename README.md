# Chat Socket Ứng Dụng Mã Hóa AES Kernel Driver

Đây là dự án Chat Socket Client-Server trên Linux có hỗ trợ mã hóa đầu cuối (End-to-End Encryption) sử dụng thuật toán AES thông qua một Linux Kernel Module.

## Yêu cầu Hệ thống
- Hệ điều hành Linux (Ubuntu, Debian, v.v.)
- `gcc` compiler
- `make` và Linux Kernel Headers (để biên dịch kernel module)

---

## 1. Biên dịch và Cài đặt Driver Mã hoá AES
Kernel Module `aes_driver.ko` cung cấp thao tác mã hóa. Vì driver được đăng ký với Major Number động, bạn cần tạo file Character Device `/dev/aes_driver` thủ công sau khi load module vào nhân:

```bash
cd driver
make
sudo insmod aes_driver.ko

# Lấy Major Number của driver được cấp phát:
cat /proc/devices | grep aes_driver

# Dùng Major Number vừa tìm được (ví dụ: 240) để tạo file device:
sudo mknod /dev/aes_driver c <Major_Number> 0

# Cấp quyền đọc/ghi cho tất cả người dùng để Client có thể tương tác:
sudo chmod 666 /dev/aes_driver
```
*(Để gỡ driver khi không dùng nữa: `sudo rmmod aes_driver` và `sudo rm /dev/aes_driver`)*

---

## 2. Biên dịch và Chạy Server
Server quản lý việc đăng ký, đăng nhập và định tuyến tin nhắn giữa các phòng chat. Server **không** giải mã tin nhắn.

Mở một Terminal mới:
```bash
cd server
gcc server.c -o server -lpthread
./server
```

---

## 3. Biên dịch và Chạy Client
Client hỗ trợ menu tương tác từng bước dễ dùng: cho phép Đăng ký, Đăng nhập, Tạo và Vào phòng chat tĩnh và xem lại lịch sử. Tin nhắn bạn gửi sẽ được mã hoá tự động bằng Driver trước khi gửi đi.

Mở một hoặc nhiều Terminal mới (tương ứng với nhiều người dùng):
```bash
cd client
gcc client.c -o client -lpthread
./client
```
