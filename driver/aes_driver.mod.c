#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const char ____versions[]
__used __section("__versions") =
	"\x18\x00\x00\x00\xe1\xbe\x10\x6b"
	"_copy_to_user\0\0\0"
	"\x1c\x00\x00\x00\x9b\x5b\x0e\x76"
	"__register_chrdev\0\0\0"
	"\x20\x00\x00\x00\xe3\xae\xb7\xf3"
	"crypto_alloc_skcipher\0\0\0"
	"\x20\x00\x00\x00\xb6\x98\xf5\xee"
	"crypto_skcipher_setkey\0\0"
	"\x10\x00\x00\x00\x7e\x3a\x2c\x12"
	"_printk\0"
	"\x1c\x00\x00\x00\xc0\xfb\xc3\x6b"
	"__unregister_chrdev\0"
	"\x1c\x00\x00\x00\xc1\x29\xed\x51"
	"crypto_destroy_tfm\0\0"
	"\x18\x00\x00\x00\xc2\x9c\xc4\x13"
	"_copy_from_user\0"
	"\x14\x00\x00\x00\x45\x3a\x23\xeb"
	"__kmalloc\0\0\0"
	"\x14\x00\x00\x00\x19\xbd\xba\x43"
	"sg_init_one\0"
	"\x20\x00\x00\x00\xb6\xcd\xd3\xa6"
	"crypto_skcipher_decrypt\0"
	"\x18\x00\x00\x00\xc0\x0f\x76\xd0"
	"kfree_sensitive\0"
	"\x20\x00\x00\x00\xe4\x85\x55\xcd"
	"crypto_skcipher_encrypt\0"
	"\x1c\x00\x00\x00\xcb\xf6\xfd\xf0"
	"__stack_chk_fail\0\0\0\0"
	"\x14\x00\x00\x00\xbb\x6d\xfb\xbd"
	"__fentry__\0\0"
	"\x1c\x00\x00\x00\xca\x39\x82\x5b"
	"__x86_return_thunk\0\0"
	"\x10\x00\x00\x00\xba\x0c\x7a\x03"
	"kfree\0\0\0"
	"\x1c\x00\x00\x00\x63\xa5\x03\x4c"
	"random_kmalloc_seed\0"
	"\x18\x00\x00\x00\x9e\xf5\x2e\xb2"
	"kmalloc_caches\0\0"
	"\x18\x00\x00\x00\x79\xb6\xdc\xd8"
	"kmalloc_trace\0\0\0"
	"\x1c\x00\x00\x00\x48\x9f\xdb\x88"
	"__check_object_size\0"
	"\x18\x00\x00\x00\xf8\x1a\x94\x92"
	"module_layout\0\0\0"
	"\x00\x00\x00\x00\x00\x00\x00\x00";

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "7DC8DE33531C293B941AF1C");
