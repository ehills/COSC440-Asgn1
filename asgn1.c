/**
 * File: asgn1.c
 * Date: 13/03/2011
 * Author: Edward Hills 
 * Version: 0.9
 *
 * This is a module which serves as a virtual ramdisk which disk size is
 * limited by the amount of memory available and serves as the requirement for
 * COSC440 assignment 1 in 2012.
 *
 * Note: multiple devices and concurrent modules are not supported in this
 *       version.
 */

/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/device.h>

#define MYDEV_NAME "asgn1"
#define MYIOC_TYPE 'k'

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Edward Hills");
MODULE_DESCRIPTION("COSC440 asgn1");


/**
 * The node structure for the memory page linked list.
 */ 
typedef struct page_node_rec {
    struct list_head list;
    struct page *page;
} page_node;

typedef struct asgn1_dev_t {
    dev_t dev;            /* the device */
    struct cdev *cdev;
    struct list_head mem_list; 
    int num_pages;        /* number of memory pages this module currently holds */
    size_t data_size;     /* total data size in this module */
    atomic_t nprocs;      /* number of processes accessing this device */ 
    atomic_t max_nprocs;  /* max number of processes accessing this device */
    struct kmem_cache *cache;      /* cache memory */
    struct class *class;     /* the udev class */
    struct device *device;   /* the udev device node */
} asgn1_dev;

asgn1_dev asgn1_device;

int asgn1_major = 0;                      /* major number of module */  
int asgn1_minor = 0;                      /* minor number of module */
int asgn1_dev_count = 1;                  /* number of devices */

/**
 * This function frees all memory pages held by the module.
 */
void free_memory_pages(void) {
    page_node *curr;
    page_node *node;

    list_for_each_entry_safe(node, curr, &asgn1_device.mem_list, list) {
        if (&node->page != NULL) {
            kfree(&node->page);
        }
        list_del(&node->list);
        kfree(node);
    }

    asgn1_device.num_pages = 0;
    asgn1_device.data_size = 0;
}


/**
 * This function opens the virtual disk, if it is opened in the write-only
 * mode, all memory pages will be freed.
 */
int asgn1_open(struct inode *inode, struct file *filp) {

    atomic_inc(&asgn1_device.nprocs);
    if (atomic_read(&asgn1_device.nprocs) > atomic_read(&asgn1_device.max_nprocs)) {
        printk(KERN_ERR "(exit): Too many processes are accessing this device\n");
        return -EBUSY;
    }

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        free_memory_pages();
    }

    return 0; /* success */
}


/*
 * This function releases the virtual disk, but nothing needs to be done
 * in this case. 
 */
int asgn1_release (struct inode *inode, struct file *filp) {

    atomic_dec(&asgn1_device.nprocs);
    return 0;
}


/**
 * This function reads contents of the virtual disk and writes to the user 
 */
ssize_t asgn1_read(struct file *filp, char __user *buf, size_t count,
        loff_t *f_pos) {
    size_t size_read = 0;     /* size read from virtual disk in this function */
    size_t begin_offset;      /* the offset from the beginning of a page to
                                 start reading */
    int begin_page_no = *f_pos / PAGE_SIZE; /* the first page which contains
                                               the requested data */
    int curr_page_no = 0;     /* the current page number */
    size_t curr_size_read;    /* size read from the virtual disk in this round */
    size_t size_to_be_read;   /* size to be read in the current round in 
                                 while loop */

    struct list_head *ptr = asgn1_device.mem_list.next;
    page_node *curr;

    if (*f_pos > asgn1_device.data_size) {
        return 0;
    }

    begin_offset= *f_pos % PAGE_SIZE;
    list_for_each_entry(curr, ptr, list) {
        curr_page_no = page_to_pfn(curr->page);
        if(begin_page_no <= curr_page_no) {

            do {
                size_to_be_read = PAGE_SIZE - begin_offset;
                if (size_to_be_read > (count - size_read)) {
                    size_to_be_read = count - size_read;
                }
                // not quite syntax to copy from offset of page
                curr_size_read += copy_to_user(&buf[size_read],
                                &(curr->page[begin_offset]),size_to_be_read); 
                begin_offset+= curr_size_read;
                size_read += curr_size_read;
            } while (curr_size_read < size_to_be_read);
            if (size_read == count) {
                return 0;
            }
            begin_offset=0;
        }
    }

    return size_read;
}


static loff_t asgn1_lseek (struct file *file, loff_t offset, int cmd)
{
    loff_t testpos;

    size_t buffer_size = asgn1_device.num_pages * PAGE_SIZE;

    testpos = cmd;
    
    if (testpos > buffer_size) {
        testpos = buffer_size;
    } else if (testpos < 0) {
        testpos = 0;
    }
    file->f_pos = testpos;

    printk (KERN_INFO "Seeking to pos=%ld\n", (long)testpos);
    return testpos;
}


/**
 * This function writes from the user buffer to the virtual disk of this
 * module
 */
ssize_t asgn1_write(struct file *filp, const char __user *buf, size_t count,
        loff_t *f_pos) {
    size_t orig_f_pos = *f_pos;  /* the original file position */
    size_t size_written = 0;  /* size written to virtual disk in this function */
    size_t begin_offset;      /* the offset from the beginning of a page to
                                 start writing */
    int begin_page_no = *f_pos / PAGE_SIZE;  /* the first page this finction
                                                should start writing to */

    int curr_page_no = 0;     /* the current page number */
    size_t curr_size_written; /* size written to virtual disk in this round */
    size_t size_to_be_written;  /* size to be read in the current round in 
                                   while loop */

    struct list_head *ptr = asgn1_device.mem_list.next;
    page_node *curr;

    // if there is no page there at all
    if (orig_f_pos > asgn1_device.data_size) {
        return 0;
    }

    list_for_each_entry(curr, ptr, list) {

        curr_page_no = page_to_pfn(curr->page);
        if (begin_page_no == curr_page_no) {
            begin_offset = orig_f_pos % PAGE_SIZE;
        }
    }

    while (size_written != count) {
        
        do {
            size_to_be_written = PAGE_SIZE - begin_offset;
            if (size_to_be_written > (count - size_written)) {
                size_to_be_written = (count - size_written);
            }
            curr_size_written += copy_from_user(page_address(curr->page) + begin_offset, &buf[size_written], size_to_be_written);
            begin_offset += curr_size_written;
            size_written += curr_size_written;
        } while (size_written > curr_size_written);
        begin_offset = 0;
        
        if ((curr->list.next == &asgn1_device.mem_list) && (count != size_written)) { // i need to add a page
            if ((curr = kmalloc(sizeof(page_node), GFP_KERNEL)) == NULL) {
                printk(KERN_ERR "Not enough memory left\n");
                return (count - size_written);
            }
            
            if ((curr->page = alloc_page(GFP_KERNEL)) == NULL) {
                printk(KERN_WARNING "Not enough memory left\n");
                asgn1_device.data_size+= size_written;
                return (count - size_written);
            }
            list_add_tail(&(curr->list), &asgn1_device.mem_list);
            asgn1_device.num_pages++;
        }
    }

    asgn1_device.data_size = max(asgn1_device.data_size,
            orig_f_pos + size_written);
    return 0;
}

#define SET_NPROC_OP 1
#define TEM_SET_NPROC _IOW(MYIOC_TYPE, SET_NPROC_OP, sizeof(int)) 

/**
 * The ioctl function, which nothing needs to be done in this case.
 */
long asgn1_ioctl (struct file *filp, unsigned cmd, unsigned long arg) {
    int nr;
    int new_nprocs;
    int result;

    if (_IOC_TYPE(cmd) != MYIOC_TYPE) {

        printk(KERN_WARNING "Invalid comand CMD=%d, for this type.\n", cmd);

        return -EINVAL;

    }
    
     /* get command, and if command is SET_NPROC_OP, then get the data, and
     set max_nprocs accordingly, don't forget to check validity of the 
     value before setting max_nprocs
     */

    return -ENOTTY;
}


/**
 * Displays information about current status of the module,
 * which helps debugging.
 */
int asgn1_read_procmem(char *buf, char **start, off_t offset, int count,
        int *eof, void *data) {
    /* stub */
    int result;

    /* COMPLETE ME */
    /**
     * use snprintf to print some info to buf, up to size count
     * set eof
     */
    return result;
}


static int asgn1_mmap (struct file *filp, struct vm_area_struct *vma)
{
    unsigned long pfn;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long len = vma->vm_end - vma->vm_start;
    unsigned long ramdisk_size = asgn1_device.num_pages * PAGE_SIZE;
    page_node *curr;
    unsigned long index = 0;

    /* COMPLETE ME */
    /**
     * check offset and len
     *
     * loop through the entire page list, once the first requested page
     *   reached, add each page with remap_pfn_range one by one
     *   up to the last requested page
     */
    return 0;
}


struct file_operations asgn1_fops = {
    .owner = THIS_MODULE,
    .read = asgn1_read,
    .write = asgn1_write,
    .unlocked_ioctl = asgn1_ioctl,
    .open = asgn1_open,
    .mmap = asgn1_mmap,
    .release = asgn1_release,
    .llseek = asgn1_lseek
};


/**
 * Initialise the module and create the master device
 */
int __init asgn1_init_module(void){
    int result; 

    /* COMPLETE ME */
    /**
     * set nprocs and max_nprocs of the device
     *
     * allocate major number
     * allocate cdev, and set ops and owner field 
     * add cdev
     * initialize the page list
     * create proc entries
     */

    asgn1_device.class = class_create(THIS_MODULE, MYDEV_NAME);
    if (IS_ERR(asgn1_device.class)) {
        printk(KERN_WARNING "%s: can't create udev class\n", MYDEV_NAME);
        result = -ENOMEM;
        goto fail_class;
    }

    asgn1_device.device = device_create(asgn1_device.class, NULL, 
            asgn1_device.dev, "%s", MYDEV_NAME);
    if (IS_ERR(asgn1_device.device)) {
        printk(KERN_WARNING "%s: can't create udev device\n", MYDEV_NAME);
        result = -ENOMEM;
        goto fail_device;
    }

    printk(KERN_WARNING "set up udev entry\n");
    printk(KERN_WARNING "Hello world from %s\n", MYDEV_NAME);
    return 0;

    /* cleanup code called when any of the initialization steps fail */
fail_device:
    class_destroy(asgn1_device.class);

    /* COMPLETE ME */
    /* PLEASE PUT YOUR CLEANUP CODE HERE, IN REVERSE ORDER OF ALLOCATION */

    return result;
}


/**
 * Finalise the module
 */
void __exit asgn1_exit_module(void){
    device_destroy(asgn1_device.class, asgn1_device.dev);
    class_destroy(asgn1_device.class);
    printk(KERN_WARNING "cleaned up udev entry\n");

    /* COMPLETE ME */
    /**
     * free all pages in the page list 
     * cleanup in reverse order
     */
    printk(KERN_WARNING "Good bye from %s\n", MYDEV_NAME);
}


module_init(asgn1_init_module);
module_exit(asgn1_exit_module);


