#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/crypto.h>
#include <crypto/skcipher.h>
#include <linux/scatterlist.h>

#define DEVICE_NAME "aes_driver"
#define KEY "1234567890123456"

static int major;
static struct crypto_skcipher *skcipher;
static char kernel_buffer[256];

static ssize_t aes_write(struct file *file,
                         const char __user *buf,
                         size_t len,
                         loff_t *off)
{
    struct skcipher_request *req;
    struct scatterlist sg;
    char key[16] = KEY;

    if(copy_from_user(kernel_buffer, buf, len))
        return -EFAULT;

    req = skcipher_request_alloc(skcipher, GFP_KERNEL);
    if(!req)
        return -ENOMEM;

    sg_init_one(&sg, kernel_buffer, len);

    crypto_skcipher_setkey(skcipher, key, 16);
    skcipher_request_set_crypt(req, &sg, &sg, len, NULL);

    crypto_skcipher_encrypt(req);

    skcipher_request_free(req);

    printk(KERN_INFO "AES encrypted data\n");

    return len;
}

static ssize_t aes_read(struct file *file,
                        char __user *buf,
                        size_t len,
                        loff_t *off)
{
    if(copy_to_user(buf, kernel_buffer, len))
        return -EFAULT;

    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = aes_write,
    .read = aes_read
};

static int __init aes_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &fops);

    skcipher = crypto_alloc_skcipher("ecb(aes)", 0, 0);
    if(IS_ERR(skcipher))
    {
        printk(KERN_ERR "Cannot load AES cipher\n");
        return PTR_ERR(skcipher);
    }

    printk(KERN_INFO "AES Driver loaded\n");
    printk(KERN_INFO "Major number: %d\n", major);

    return 0;
}

static void __exit aes_exit(void)
{
    crypto_free_skcipher(skcipher);
    unregister_chrdev(major, DEVICE_NAME);

    printk(KERN_INFO "AES Driver removed\n");
}

module_init(aes_init);
module_exit(aes_exit);

MODULE_LICENSE("GPL");