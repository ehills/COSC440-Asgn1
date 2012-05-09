#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x41572473, "module_layout" },
	{ 0x3ec8886f, "param_ops_int" },
	{ 0x22bdcf2b, "device_destroy" },
	{ 0x22c2a189, "kmem_cache_destroy" },
	{ 0x56ce0b62, "remove_proc_entry" },
	{ 0x4474c46a, "class_destroy" },
	{ 0xf946e034, "device_create" },
	{ 0xe74b1a5a, "__class_create" },
	{ 0xa301657f, "create_proc_entry" },
	{ 0x1ac0e406, "kmem_cache_create" },
	{ 0xe81fd0f2, "cdev_del" },
	{ 0x8e998634, "cdev_add" },
	{ 0xd606b81f, "cdev_init" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0xf5c1c7d5, "cdev_alloc" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0xf731a82b, "kmem_cache_free" },
	{ 0xc9d01606, "__free_pages" },
	{ 0x6729d3df, "__get_user_4" },
	{ 0x4f8b5ddb, "_copy_to_user" },
	{ 0x6a2b6e57, "alloc_pages_current" },
	{ 0xfdcfd190, "kmem_cache_alloc" },
	{ 0x4f6b400b, "_copy_from_user" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0x9edbecae, "snprintf" },
	{ 0xc9504f52, "remap_pfn_range" },
	{ 0x27e1a049, "printk" },
	{ 0xb4390f9a, "mcount" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "C2144B655B8746CE6814AE6");
