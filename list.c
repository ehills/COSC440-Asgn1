/* **************** LDD:1.0 s_07/lab1_list.c **************** */
/*
 * The code herein is: Copyright Jerry Cooperstein, 2009
 *
 * This Copyright is retained for the purpose of protecting free
 * redistribution of source.
 *
 *     URL:    http://www.coopj.com
 *     email:  coop@coopj.com
 *
 * The primary maintainer for this code is Jerry Cooperstein
 * The CONTRIBUTORS file (distributed with this
 * file) lists those known to have contributed to the source.
 *
 * This code is distributed under Version 2 of the GNU General Public
 * License, which you should have received with the source.
 *
 */
/* Linked lists
 *
 * Write a module that sets up a doubly-linked circular list of data
 * structures.  The data structure can be as simple as an integer
 * variable.
 *
 * Test inserting and deleting elements in the list.
 *
 * Walk through the list (using list_entry()) and print out values to
 * make sure the insertion and deletion processes are working.
 @*/

#include <linux/module.h>
#include <asm/atomic.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/init.h>

static LIST_HEAD(my_list);
static int page_size = PAGE_SIZE;
static struct kmem_cache *my_cache;
module_param(page_size, int, S_IRUGO);

struct my_entry {
    struct list_head clist;	
    int val;
};

static int __init my_init(void)
{
    struct my_entry *ce;
    struct my_entry *iter;
    int k;

    // allocate room for struct

    for (k =0; k < 5; k++) {
        if (!(ce = kmalloc(sizeof(struct my_entry), GFP_KERNEL))) {
            printk(KERN_INFO " Failed to allocate memory for struct %d\n", k);
            return -ENOMEM;
        }

        ce->val = 11 + k;
        printk(KERN_INFO" adding val %d to my_list\n", ce->val);
        list_add(&ce->clist, &my_list);

    }

        printk(KERN_INFO "lists created\n");
    /*
       if (!(ce = kmem_cache_alloc(my_cache, GFP_KERNEL))) {
       printk(KERN_ERR " failed to create a cache object\n");
       return -ENOMEM;
       }
       printk(KERN_INFO " successfully created a cache object\n");
     */

    //traverse
    list_for_each_entry(iter,&my_list,clist) {
        printk(KERN_INFO "list val=%d\n", iter->val);
    }

    return 0;
}

static void __exit my_exit(void)
{
    struct my_entry *entry;	/* pointer to list head object */
    struct my_entry *tmp;	/* temporary list head for safe deletion */

    if (list_empty(&my_list)) {
        printk(KERN_INFO "(exit): list is empty!\n");
        return;
    }
    printk(KERN_INFO "(exit): list is not empty!\n");
    
    list_for_each_entry_safe(entry, tmp, &my_list, clist) {
        list_del(&entry->clist);
        printk(KERN_INFO "(exit): List entry removed. Contained Val: %d\n", entry->val);
        kfree(entry);
    }
    list_del(&my_list);

    if (list_empty(&my_list)) {
        printk(KERN_INFO "lists destroyed\n");
    } else {
        printk(KERN_INFO "lists not destroyed as they are not empty\nThese were lefty:\n");
        list_for_each_entry(entry,&my_list,clist) {
            printk(KERN_INFO "list val=%d\n", entry->val);
        }
    } 
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Dave Harris");
/* many modifications by Jerry Cooperstein */
/* many modifications by Ed Hills */
MODULE_DESCRIPTION("LDD:1.0 s_07/lab1_list.c");
MODULE_LICENSE("GPL v2");
