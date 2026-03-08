#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <crypto/skcipher.h>
#include <linux/scatterlist.h>

#define DEVICE "aes_driver"             // Tên thiết bị ảo
#define KEY "1234567890123456"          // Chìa khóa mã hóa (16 byte tương ứng AES-128)

#define AES_ENCRYPT 0                   // Chế độ mã hóa
#define AES_DECRYPT 1                   // Chế độ giải mã

static int major;                        // Biến lưu trữ "Major number" dùng để định danh thiết bị này trong hệ thống
static struct crypto_skcipher *tfm;      // Con trỏ đối tượng mã hóa, giữ thuật toán AES

// Cấu trúc lưu context của một ứng dụng khi nó mở kết nối với driver
struct aes_ctx {
    int mode;                            // Chế độ hoạt động (AES_DECRYPT hay AES_ENCRYPT)
    char buffer[256];                    // Bộ đệm chứa dữ liệu (tối đa 256 byte)
};

// Hàm được gọi khi ứng dụng dùng lệnh open("/dev/aes_driver", ...)
static int dev_open(struct inode *inode, struct file *f)
{
    // Cấp phát bộ nhớ tạo ra một hộp chứa (context) dành riêng cho ứng dụng đó
    struct aes_ctx *ctx = kmalloc(sizeof(struct aes_ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;
    
    ctx->mode = AES_ENCRYPT;     // Mặc định là mã hóa
    memset(ctx->buffer, 0, 256); // Xóa sạch dữ liệu cũ trong buffer
    f->private_data = ctx;       // Lưu hộp chứa này lại để các hàm khác có thể dùng
    
    return 0;
}

// Hàm được gọi khi ứng dụng dùng lệnh close(fd)
static int dev_release(struct inode *inode, struct file *f)
{
    struct aes_ctx *ctx = f->private_data;
    if (ctx) {
        kfree(ctx); // Dọn dẹp, giải phóng bộ nhớ của hộp chứa (context)
    }
    return 0;
}

// Được gọi khi ứng dụng thực thi lệnh write(fd, msg, 256) (Gửi dữ liệu thô xuống kernel)
static ssize_t dev_write(struct file *f,
                         const char __user *buf,
                         size_t len,
                         loff_t *off)
{
    struct skcipher_request *req;
    struct scatterlist sg;
    struct aes_ctx *ctx = f->private_data;

    // Giới hạn max là 256 byte
    if (len > 256) len = 256;

    // Copy an toàn dữ liệu từ ngoài ứng dụng (User space) vào trong nhân (Kernel space)
    if (copy_from_user(ctx->buffer, buf, len))
        return -EFAULT;

    // Yêu cầu cấp lấy một đối tượng request để thực hiện mã hóa
    req = skcipher_request_alloc(tfm, GFP_KERNEL);
    if (!req)
        return -ENOMEM;

    // Nạp dữ liệu vào bảng phân tán (Scatterlist)
    sg_init_one(&sg, ctx->buffer, len);
    
    // Yêu cầu mã hóa và giải mã sẽ ghi đè thẳng kết quả lên cùng vùng nhớ (&sg, &sg)
    skcipher_request_set_crypt(req, &sg, &sg, len, NULL);

    // Dựa vào mode do người dùng set mà mã hóa hoặc giải mã
    if (ctx->mode == AES_ENCRYPT)
        crypto_skcipher_encrypt(req);
    else
        crypto_skcipher_decrypt(req);

    // Xong việc thì dọn đổi tượng request đi
    skcipher_request_free(req);

    return len;
}

// Được gọi khi ứng dụng chạy lệnh read(fd, msg, 256) (Lấy kết quả về)
static ssize_t dev_read(struct file *f,
                        char __user *buf,
                        size_t len,
                        loff_t *off)
{
    struct aes_ctx *ctx = f->private_data;
    if (len > 256) len = 256;
    
    // Đẩy dữ liệu đã được xử lý xong trong buffer (Kernel) trả ngược về cho biến của ứng dụng (User)
    if (copy_to_user(buf, ctx->buffer, len))
        return -EFAULT;
        
    return len;
}

// Được gọi khi ứng dụng thiết lập cấu hình thông qua ioctl(fd, mode)
static long dev_ioctl(struct file *f,
                      unsigned int cmd,
                      unsigned long arg)
{
    struct aes_ctx *ctx = f->private_data;
    ctx->mode = cmd; // Thay đổi chế độ lưu trong context (Mã hóa = 0 hoặc Giải mã = 1)
    return 0;
}

// Cấu trúc liên kết các hoạt động của User tới hàm tương ứng trong Kernel
static struct file_operations fops = {
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
    .unlocked_ioctl = dev_ioctl
};

// Hàm khởi tạo Driver (Chạy khi dùng lệnh cài driver 'insmod aes_driver.ko')
static int __init aes_init(void)
{
    // Đăng ký thiết bị và nhận 1 mã số định danh riêng (Major)
    major = register_chrdev(0, DEVICE, &fops);
    if (major < 0) {
        printk(KERN_ERR "AES driver: failed to register major number\n");
        return major;
    }

    // Khởi tạo thuật toán theo chuẩn chế độ ecb(aes)
    tfm = crypto_alloc_skcipher("ecb(aes)", 0, 0);
    if (IS_ERR(tfm)) {
        printk(KERN_ERR "AES driver: failed to allocate skcipher\n");
        unregister_chrdev(major, DEVICE);
        return PTR_ERR(tfm);
    }

    // Dùng khóa tĩnh 16 ký tự ở trên nạp vào là khóa AES chung
    if (crypto_skcipher_setkey(tfm, KEY, 16)) {
        printk(KERN_ERR "AES driver: failed to set key\n");
        crypto_free_skcipher(tfm);
        unregister_chrdev(major, DEVICE);
        return -EAGAIN;
    }

    printk(KERN_INFO "AES driver loaded with major=%d\n", major);
    return 0;
}

// Hàm gỡ bỏ Driver (Chạy khi dùng lệnh tháo driver 'rmmod aes_driver')
static void __exit aes_exit(void)
{
    crypto_free_skcipher(tfm);        // Dọn dẹp cơ chế cấp phát Crypto
    unregister_chrdev(major, DEVICE); // Kêu hệ điều hành hủy ghi nhận tên thiết bị
    printk(KERN_INFO "AES driver unloaded\n");
}

module_init(aes_init);
module_exit(aes_exit);

MODULE_LICENSE("GPL");