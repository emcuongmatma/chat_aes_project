#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>

#define DEVICE "aes_driver"             // Tên file thiết bị ảo trong thư mục /dev/
#define KEY_STRING "1234567890123456"   // Khóa bảo mật tĩnh (16 byte tương đương AES-128)

#define AES_ENCRYPT 0                   // Mode: Mã hóa
#define AES_DECRYPT 1                   // Mode: Giải mã

static int major;                        // Biến lưu số định danh đăng ký với nhân hệ điều hành

// Cấu trúc lưu "trạng thái" mỗi khi có một ứng dụng kết nối tới driver
struct aes_ctx {
    int mode;                            // Đang yêu cầu mã hóa hay giải mã
    unsigned char buffer[256];           // Bộ nhớ đệm chứa dữ liệu
};

// ===============================================
// CÁC BẢNG TRA CỨU TIÊU CHUẨN CỦA AES
// ===============================================

// Bảng S-box dùng để xáo trộn byte trong pha SubBytes
static const unsigned char sbox[256] = {
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const unsigned char rsbox[256] = {
  0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
  0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
  0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
  0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
  0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
  0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
  0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
  0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
  0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
  0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
  0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
  0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
  0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
  0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
  0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
  0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

// Bảng Rcon dùng để sinh khóa vòng (Key Expansion)
static const unsigned char Rcon[11] = {
  0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

// Hàm mở rộng khóa (Key Expansion): Sinh ra 11 "Khóa vòng" từ Khóa chính ban đầu
static void KeyExpansion(unsigned char* RoundKey, const unsigned char* Key) {
    unsigned i, j, k;
    unsigned char tempa[4];

    for (i = 0; i < 4; ++i) {
        RoundKey[(i * 4) + 0] = Key[(i * 4) + 0];
        RoundKey[(i * 4) + 1] = Key[(i * 4) + 1];
        RoundKey[(i * 4) + 2] = Key[(i * 4) + 2];
        RoundKey[(i * 4) + 3] = Key[(i * 4) + 3];
    }

    for (i = 4; i < 44; ++i) {
        k = (i - 1) * 4;
        tempa[0] = RoundKey[k + 0];
        tempa[1] = RoundKey[k + 1];
        tempa[2] = RoundKey[k + 2];
        tempa[3] = RoundKey[k + 3];

        if (i % 4 == 0) {
            const unsigned char u8tmp = tempa[0];
            tempa[0] = sbox[tempa[1]];
            tempa[1] = sbox[tempa[2]];
            tempa[2] = sbox[tempa[3]];
            tempa[3] = sbox[u8tmp];
            tempa[0] = tempa[0] ^ Rcon[i/4];
        }
        
        j = i * 4; k = (i - 4) * 4;
        RoundKey[j + 0] = RoundKey[k + 0] ^ tempa[0];
        RoundKey[j + 1] = RoundKey[k + 1] ^ tempa[1];
        RoundKey[j + 2] = RoundKey[k + 2] ^ tempa[2];
        RoundKey[j + 3] = RoundKey[k + 3] ^ tempa[3];
    }
}

// Bước AddRoundKey: XOR trạng thái hiện tại với khóa của Vòng tương ứng
static void AddRoundKey(unsigned char round, unsigned char state[4][4], const unsigned char* RoundKey) {
    unsigned char i, j;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            state[i][j] ^= RoundKey[(round * 16) + (i * 4) + j];
        }
    }
}

// Bước SubBytes: Thay thế từng byte bằng bảng S-box để tạo tính phi tuyến tính
static void SubBytes(unsigned char state[4][4]) {
    unsigned char i, j;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            state[j][i] = sbox[state[j][i]];
        }
    }
}

// Bước ShiftRows: Dịch vòng các hàng để xáo trộn vị trí
static void ShiftRows(unsigned char state[4][4]) {
    unsigned char temp;
    temp           = state[0][1];
    state[0][1] = state[1][1];
    state[1][1] = state[2][1];
    state[2][1] = state[3][1];
    state[3][1] = temp;

    temp           = state[0][2];
    state[0][2] = state[2][2];
    state[2][2] = temp;
    temp           = state[1][2];
    state[1][2] = state[3][2];
    state[3][2] = temp;

    temp           = state[0][3];
    state[0][3] = state[3][3];
    state[3][3] = state[2][3];
    state[2][3] = state[1][3];
    state[1][3] = temp;
}

// Hàm tính phép nhân trong trường hữu hạn Galois GF(2^8) (phục vụ MixColumns)
static unsigned char xtime(unsigned char x) {
    return ((x<<1) ^ (((x>>7) & 1) * 0x1b));
}

// Bước MixColumns: Xáo trộn các cột bằng phép nhân ma trận trên trường Galois
static void MixColumns(unsigned char state[4][4]) {
    unsigned char i;
    unsigned char Tmp, Tm, t;
    for (i = 0; i < 4; ++i) {  
        t   = state[i][0];
        Tmp = state[i][0] ^ state[i][1] ^ state[i][2] ^ state[i][3];
        Tm  = state[i][0] ^ state[i][1]; Tm = xtime(Tm); state[i][0] ^= Tm ^ Tmp;
        Tm  = state[i][1] ^ state[i][2]; Tm = xtime(Tm); state[i][1] ^= Tm ^ Tmp;
        Tm  = state[i][2] ^ state[i][3]; Tm = xtime(Tm); state[i][2] ^= Tm ^ Tmp;
        Tm  = state[i][3] ^ t;           Tm = xtime(Tm); state[i][3] ^= Tm ^ Tmp;
    }
}

static void Multiply(unsigned char x, unsigned char y, unsigned char *res) {
    unsigned char sum = 0;
    unsigned char i;
    for(i=0; i<8; i++) {
        if((y & 1)) sum ^= x;
        unsigned char hi = x & 0x80;
        x <<= 1;
        if(hi) x ^= 0x1b;
        y >>= 1;
    }
    *res = sum;
}

static void InvMixColumns(unsigned char state[4][4]) {
    int i;
    unsigned char a, b, c, d;
    unsigned char ma, mb, mc, md;
    for (i = 0; i < 4; ++i) { 
        a = state[i][0]; b = state[i][1]; c = state[i][2]; d = state[i][3];

        Multiply(a, 0x0e, &ma); Multiply(b, 0x0b, &mb); Multiply(c, 0x0d, &mc); Multiply(d, 0x09, &md);
        state[i][0] = ma ^ mb ^ mc ^ md;
        Multiply(a, 0x09, &ma); Multiply(b, 0x0e, &mb); Multiply(c, 0x0b, &mc); Multiply(d, 0x0d, &md);
        state[i][1] = ma ^ mb ^ mc ^ md;
        Multiply(a, 0x0d, &ma); Multiply(b, 0x09, &mb); Multiply(c, 0x0e, &mc); Multiply(d, 0x0b, &md);
        state[i][2] = ma ^ mb ^ mc ^ md;
        Multiply(a, 0x0b, &ma); Multiply(b, 0x0d, &mb); Multiply(c, 0x09, &mc); Multiply(d, 0x0e, &md);
        state[i][3] = ma ^ mb ^ mc ^ md;
    }
}

static void InvSubBytes(unsigned char state[4][4]) {
    unsigned char i, j;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            state[j][i] = rsbox[state[j][i]];
        }
    }
}

static void InvShiftRows(unsigned char state[4][4]) {
    unsigned char temp;
    temp = state[3][1];
    state[3][1] = state[2][1];
    state[2][1] = state[1][1];
    state[1][1] = state[0][1];
    state[0][1] = temp;

    temp = state[0][2];
    state[0][2] = state[2][2];
    state[2][2] = temp;

    temp = state[1][2];
    state[1][2] = state[3][2];
    state[3][2] = temp;

    temp = state[0][3];
    state[0][3] = state[1][3];
    state[1][3] = state[2][3];
    state[2][3] = state[3][3];
    state[3][3] = temp;
}

// Hàm tóm lược tiến trình mã hóa (10 vòng đối với AES-128)
static void Cipher(unsigned char state[4][4], const unsigned char* RoundKey) {
    unsigned char round = 0;
    AddRoundKey(0, state, RoundKey);
    for (round = 1; round < 10; ++round) {
        SubBytes(state);
        ShiftRows(state);
        MixColumns(state);
        AddRoundKey(round, state, RoundKey);
    }
    SubBytes(state);
    ShiftRows(state);
    AddRoundKey(10, state, RoundKey);
}

// Hàm tóm lược tiến trình giải mã (làm các bước ngược lại, tính từ vòng 10 lùi về 1)
static void InvCipher(unsigned char state[4][4], const unsigned char* RoundKey) {
    unsigned char round = 0;
    AddRoundKey(10, state, RoundKey);
    for (round = 9; round > 0; --round) {
        InvShiftRows(state);
        InvSubBytes(state);
        AddRoundKey(round, state, RoundKey);
        InvMixColumns(state);
    }
    InvShiftRows(state);
    InvSubBytes(state);
    AddRoundKey(0, state, RoundKey);
}

// API Giao diện thực thi Mã hóa 1 block (16 bytes)
static void AES128_encrypt(unsigned char* input, const unsigned char* key, unsigned char* output) {
    unsigned char RoundKey[176];
    KeyExpansion(RoundKey, key);
    memcpy(output, input, 16);
    Cipher((unsigned char (*)[4])output, RoundKey);
}

// API Giao diện thực thi Giải mã 1 block (16 bytes)
static void AES128_decrypt(unsigned char* input, const unsigned char* key, unsigned char* output) {
    unsigned char RoundKey[176];
    KeyExpansion(RoundKey, key);
    memcpy(output, input, 16);
    InvCipher((unsigned char (*)[4])output, RoundKey);
}

// ===============================================
// KERNEL MODULE FILE OPERATIONS (GIAO THỨC DRIVER)
// ===============================================

// Hàm được tự động gọi khi ứng dụng chạy open("/dev/aes_driver")
static int dev_open(struct inode *inode, struct file *f) {
    struct aes_ctx *ctx = kmalloc(sizeof(struct aes_ctx), GFP_KERNEL);
    if (!ctx) return -ENOMEM;
    ctx->mode = AES_ENCRYPT;
    memset(ctx->buffer, 0, 256);
    f->private_data = ctx;
    return 0;
}

// Hàm được tự động gọi khi ứng dụng chạy close()
static int dev_release(struct inode *inode, struct file *f) {
    struct aes_ctx *ctx = f->private_data;
    if (ctx) kfree(ctx);
    return 0;
}

// Hàm được gọi khi ứng dụng ném dữ liệu xuống cho Kernel xử lý thông qua write()
static ssize_t dev_write(struct file *f, const char __user *buf, size_t len, loff_t *off) {
    struct aes_ctx *ctx = f->private_data;
    int i, blocks;

    if (len > 256) len = 256;
    // Copy dữ liệu chưa xử lý từ App (User) vào Buffer (Kernel)
    if (copy_from_user(ctx->buffer, buf, len)) return -EFAULT;

    // AES hoạt động theo khối (Block cipher) -> mỗi khối chuẩn 16 byte
    // Tính tổng số khối cần băm từ tin nhắn của ứng dụng
    blocks = len / 16;
    if (len % 16 != 0) blocks++;
    
    // Xử lý mã hóa / giải mã xoay vòng từng block một
    for (i = 0; i < blocks; i++) {
        unsigned char block[16] = {0};
        unsigned char out[16] = {0};
        
        // Trích xuất 16 byte tương ứng
        int n = (len - i * 16) >= 16 ? 16 : (len - i * 16);
        memcpy(block, ctx->buffer + i * 16, n);
        
        // Căn cứ vào mode để quyết định Mã hóa / Giải mã block này
        if (ctx->mode == AES_ENCRYPT)
            AES128_encrypt(block, (const unsigned char *)KEY_STRING, out);
        else
            AES128_decrypt(block, (const unsigned char *)KEY_STRING, out);
            
        // Lưu lại kết quả vào chính buffer (ghi đè đồ tươi bằng đồ đã luộc)
        memcpy(ctx->buffer + i * 16, out, 16);
    }

    return len;
}

// Hàm được gọi khi ứng dụng lên xin nhận kết quả về bằng tham số read()
static ssize_t dev_read(struct file *f, char __user *buf, size_t len, loff_t *off) {
    struct aes_ctx *ctx = f->private_data;
    if (len > 256) len = 256;
    if (copy_to_user(buf, ctx->buffer, len)) return -EFAULT;
    return len;
}

// Hàm được gọi khi ứng dụng gửi chỉ thị (0 lả hóa, 1 là giải mã) thông qua ioctl()
static long dev_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
    struct aes_ctx *ctx = f->private_data;
    ctx->mode = cmd;
    return 0;
}

static struct file_operations fops = {
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
    .unlocked_ioctl = dev_ioctl
};

// Khởi tạo Driver
static int __init aes_init(void) {
    major = register_chrdev(0, DEVICE, &fops);
    if (major < 0) {
        printk(KERN_ERR "AES driver: failed to register major number\n");
        return major;
    }
    printk(KERN_INFO "AES driver loaded with major=%d (Manual implementation)\n", major);
    return 0;
}

// Dỡ Module Driver
static void __exit aes_exit(void) {
    unregister_chrdev(major, DEVICE);
    printk(KERN_INFO "AES driver unloaded\n");
}

module_init(aes_init);
module_exit(aes_exit);

MODULE_LICENSE("GPL");