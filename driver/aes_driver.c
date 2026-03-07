#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <crypto/skcipher.h>
#include <linux/scatterlist.h>

#define DEVICE "aes_driver"
#define KEY "1234567890123456"

#define AES_ENCRYPT 0
#define AES_DECRYPT 1

static int major;
static struct crypto_skcipher *tfm;

struct aes_ctx {
    int mode;
    char buffer[256];
};

static int dev_open(struct inode *inode, struct file *f)
{
    struct aes_ctx *ctx = kmalloc(sizeof(struct aes_ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;
    
    ctx->mode = AES_ENCRYPT;
    memset(ctx->buffer, 0, 256);
    f->private_data = ctx;
    
    return 0;
}

static int dev_release(struct inode *inode, struct file *f)
{
    struct aes_ctx *ctx = f->private_data;
    if (ctx) {
        kfree(ctx);
    }
    return 0;
}

static ssize_t dev_write(struct file *f,
                         const char __user *buf,
                         size_t len,
                         loff_t *off)
{
    struct skcipher_request *req;
    struct scatterlist sg;
    struct aes_ctx *ctx = f->private_data;

    if (len > 256) len = 256;

    if (copy_from_user(ctx->buffer, buf, len))
        return -EFAULT;

    req = skcipher_request_alloc(tfm, GFP_KERNEL);
    if (!req)
        return -ENOMEM;

    sg_init_one(&sg, ctx->buffer, len);
    
    skcipher_request_set_crypt(req, &sg, &sg, len, NULL);

    if (ctx->mode == AES_ENCRYPT)
        crypto_skcipher_encrypt(req);
    else
        crypto_skcipher_decrypt(req);

    skcipher_request_free(req);

    return len;
}

static ssize_t dev_read(struct file *f,
                        char __user *buf,
                        size_t len,
                        loff_t *off)
{
    struct aes_ctx *ctx = f->private_data;
    if (len > 256) len = 256;
    if (copy_to_user(buf, ctx->buffer, len))
        return -EFAULT;
    return len;
}

static long dev_ioctl(struct file *f,
                      unsigned int cmd,
                      unsigned long arg)
{
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

static int __init aes_init(void)
{
    major = register_chrdev(0, DEVICE, &fops);
    if (major < 0) {
        printk(KERN_ERR "AES driver: failed to register major number\n");
        return major;
    }

    tfm = crypto_alloc_skcipher("ecb(aes)", 0, 0);
    if (IS_ERR(tfm)) {
        printk(KERN_ERR "AES driver: failed to allocate skcipher\n");
        unregister_chrdev(major, DEVICE);
        return PTR_ERR(tfm);
    }

    if (crypto_skcipher_setkey(tfm, KEY, 16)) {
        printk(KERN_ERR "AES driver: failed to set key\n");
        crypto_free_skcipher(tfm);
        unregister_chrdev(major, DEVICE);
        return -EAGAIN;
    }

    printk(KERN_INFO "AES driver loaded with major=%d\n", major);
    return 0;
}

static void __exit aes_exit(void)
{
    crypto_free_skcipher(tfm);
    unregister_chrdev(major, DEVICE);
    printk(KERN_INFO "AES driver unloaded\n");
}

module_init(aes_init);
module_exit(aes_exit);

MODULE_LICENSE("GPL");