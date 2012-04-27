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
	{ 0xcbef08d, "module_layout" },
	{ 0xca16e994, "cdev_alloc" },
	{ 0xcf2e1d53, "cdev_del" },
	{ 0xb4c2205e, "kmalloc_caches" },
	{ 0xbcc5ddab, "cdev_init" },
	{ 0xdd74c96d, "mem_map" },
	{ 0x6980fe91, "param_get_int" },
	{ 0xc6341751, "page_address" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0x105e2727, "__tracepoint_kmalloc" },
	{ 0xc274e480, "remove_proc_entry" },
	{ 0xf53ed33d, "device_destroy" },
	{ 0x6729d3df, "__get_user_4" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0xff964b25, "param_set_int" },
	{ 0xdfc06181, "__alloc_pages_nodemask" },
	{ 0xb72397d5, "printk" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0x2f287f0d, "copy_to_user" },
	{ 0xb4390f9a, "mcount" },
	{ 0x374a690c, "device_create" },
	{ 0x96af1a48, "contig_page_data" },
	{ 0x8b823657, "cdev_add" },
	{ 0xb06d95d7, "kmem_cache_alloc" },
	{ 0xf7ad06e5, "__free_pages" },
	{ 0x43a6b9f6, "create_proc_entry" },
	{ 0x37a0cba, "kfree" },
	{ 0x6b73547, "remap_pfn_range" },
	{ 0xb8857caf, "class_destroy" },
	{ 0x701d0ebd, "snprintf" },
	{ 0x20e01bc9, "__class_create" },
	{ 0xd6c963c, "copy_from_user" },
	{ 0x29537c9e, "alloc_chrdev_region" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "0FE4FB11761581752FB1135");
