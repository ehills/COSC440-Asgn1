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

module_param(asgn1_major, int, S_IRUGO);
MODULE_PARM_DESC(asgn1_major, "device major number");

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

    if (atomic_read(&asgn1_device.nprocs) >= atomic_read(&asgn1_device.max_nprocs)) {
        printk(KERN_ERR "(exit): Too many processes are accessing this device\n");
        return -EBUSY;
    }
    atomic_inc(&asgn1_device.nprocs);

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        free_memory_pages();
    }
    printk(KERN_INFO " attempting to open device: %s\n", MYDEV_NAME);
    printk(KERN_INFO " MAJOR number = %d, MINOR number = %d\n",
            imajor(inode), iminor(inode));
    return 0; /* success */
}


/*
 * This function releases the virtual disk, but nothing needs to be done
 * in this case. 
 */
int asgn1_release (struct inode *inode, struct file *filp) {

    atomic_dec(&asgn1_device.nprocs);
    printk(KERN_INFO " closing character device: %s\n\n", MYDEV_NAME);
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
        printk(KERN_ERR "Reached end of the device on a read");
        return -EFAULT;
    }
        
    begin_offset = *f_pos % PAGE_SIZE;
    list_for_each_entry(curr, ptr, list) {
        curr_page_no = page_to_pfn(curr->page);
        printk(KERN_ERR "being page: %d\ncurr_page: %d\n", begin_page_no, curr_page_no);
        if (begin_page_no <= curr_page_no) {
            do {
                size_to_be_read = min((PAGE_SIZE - begin_offset),
                                                    (count - size_read));
                printk(KERN_INFO "Going to read %d", (int)size_to_be_read);
                curr_size_read = size_to_be_read - copy_to_user(buf + size_read, page_address(curr->page) + begin_offset, size_to_be_read);
                printk(KERN_INFO "I read %d bytes\nfrom %p\nwithout offset was %p", (int)curr_size_read, page_address(curr->page) + begin_offset, page_address(curr->page));
                size_read += curr_size_read;
                size_to_be_read -= curr_size_read;
                begin_offset += curr_size_read;
        break;
            } while(size_read < count);
            begin_offset = 0;
        }
    }
    return size_read;

}

/**
 * This function allows the user to seek to a certain position in the
 * device from the position specified in the cmd parameter.
 */
static loff_t asgn1_lseek (struct file *file, loff_t offset, int cmd)
{
    loff_t testpos;

    size_t buffer_size = asgn1_device.num_pages * PAGE_SIZE;

    switch(cmd) {
        case SEEK_SET:
            testpos = offset;
            break;
        case SEEK_CUR:
            testpos = file->f_pos + offset;
        case SEEK_END:
            testpos = asgn1_device.data_size + offset;
            break;
        default:
            return -EINVAL;
    }    

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
    int begin_page_no = *f_pos / PAGE_SIZE;  /* the first page this function
                                                should start writing to */

    int curr_page_no = 0;     /* the current page number */
    size_t curr_size_written; /* size written to virtual disk in this round */
    size_t size_to_be_written;  /* size to be read in the current round in 
                                   while loop */

    struct list_head *ptr = asgn1_device.mem_list.next;
    page_node *curr;

    if (orig_f_pos > asgn1_device.data_size) {
        printk(KERN_WARNING "Reached end of the device on a read");
        return 0;
    }

//    if (ptr == &(asgn1_device.mem_list)) { // not needed because when opened
//                                           // for write it will clear ALL pages   
        printk(KERN_INFO "The page list is empty so will add a new page now.");
        if ((curr = kmalloc(sizeof(page_node), GFP_KERNEL)) == NULL) {
            printk(KERN_ERR "Not enough memory left\n");
            return -ENOMEM;
        }

        if ((curr->page = alloc_page(GFP_KERNEL)) == NULL) {
            printk(KERN_WARNING "Not enough memory left\n");
            return -ENOMEM;
        }
        list_add_tail(&(curr->list), &(asgn1_device.mem_list));
        curr_page_no = page_to_pfn(curr->page);
        begin_page_no = curr_page_no;
        printk(KERN_INFO "page no is: %d", begin_page_no);
        asgn1_device.num_pages++;
        ptr = asgn1_device.mem_list.prev;
  //  }

    size_to_be_written = count;
    printk(KERN_INFO "Going to write %d bytes", (int)size_to_be_written);
    printk(KERN_INFO "Writing to address %p", page_address(curr->page));
    size_written = size_to_be_written - copy_from_user(page_address(curr->page), buf + size_written, size_to_be_written);
    printk(KERN_INFO "I wrote %d bytes", (int)size_written);
    asgn1_device.data_size += size_written;
    
    return count; 

   /* list_for_each_entry(curr, ptr, list) {
        curr_page_no = page_to_pfn(curr->page);
        printk(KERN_INFO "Page to start at: %d Page i have: %d", begin_page_no, curr_page_no);
        
        if (begin_page_no == curr_page_no) {
            begin_offset = 0;
            printk(KERN_INFO "FOUND THE PAGE TO START AT");

            while (size_written < count) {

                do {
                    
                    size_to_be_written = min((PAGE_SIZE - begin_offset),
                                                        (count - size_written));
                    curr_size_written = size_to_be_written - 
                    copy_from_user(page_address(curr->page) + begin_offset, 
                                    buf + size_written, size_to_be_written);
                    size_written += curr_size_written;
                    size_to_be_written -= curr_size_written;
                    begin_offset += curr_size_written;

                } while (curr_size_written < size_to_be_written);
                begin_offset = 0;

                if (size_written < count) {
                    if ((curr = kmalloc(sizeof(page_node), GFP_KERNEL)) == NULL) {
                        printk(KERN_ERR "Not enough memory left\n");
                        return -ENOMEM;
                    }

                    if ((curr->page = alloc_page(GFP_KERNEL)) == NULL) {
                        printk(KERN_WARNING "Not enough memory left\n");
                        return -ENOMEM;
                    }
                    list_add_tail(&(curr->list), &(asgn1_device.mem_list));
                    curr_page_no = page_to_pfn(curr->page);
                    asgn1_device.num_pages++;
                }
            }
        }
    }
    *f_pos += size_written;
    asgn1_device.data_size = max(asgn1_device.data_size,
            orig_f_pos + size_written);
    printk(KERN_ERR "I wrote %d", (int)size_written);
    return 0;
    */
} 

#define SET_NPROC_OP 1
#define TEM_SET_NPROC _IOW(MYIOC_TYPE, SET_NPROC_OP, sizeof(int)) 

/**
 * The ioctl function, which nothing needs to be done in this case.
 */
long asgn1_ioctl (struct file *filp, unsigned int cmd, unsigned long arg) {
    int nr;
    int new_nprocs;
    int result;

    if (_IOC_TYPE(cmd) != MYIOC_TYPE) {
        printk(KERN_WARNING "Invalid comand CMD=%d, for this type.\n", cmd);
        return -EINVAL;
    }

    // TODO find out why its not TEM_SET_NPROC
    if (cmd == SET_NPROC_OP) {
        nr = (int)arg;
        new_nprocs = nr + atomic_read(&asgn1_device.nprocs);
        
        if (new_nprocs > atomic_read(&asgn1_device.max_nprocs) + nr) {
            printk(KERN_ERR "Cannot set maximum number of processes to %d because too many processes are currently accessing this device.\n", new_nprocs);
            result = -1;
        } else {
            atomic_set(&asgn1_device.max_nprocs, new_nprocs);
            result = 0;
        }
        return result;
    }

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

    // need to write count + 1 bytes because need extra 1 for nul
    result = snprintf(buf + offset, count + 1, "Reading from /proc/%s\n", MYDEV_NAME);  

    if (result <= offset + count) {
        *eof = 1;
    }

    *start = buf + offset;
    result -= offset;

    if (result > count)
        result = count;
    if (result < 0)
        result = 0;

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
    struct list_head *ptr = asgn1_device.mem_list.next;

    if (offset % PAGE_SIZE != 0 || offset > ramdisk_size) {
        printk(KERN_ERR "Offset must be on valid page boundary.\n");
        return -EAGAIN;
    } else if (len % PAGE_SIZE != 0) {
        printk(KERN_ERR "Length must be on a multiple of page_size.\n");
        return -EAGAIN;
    } else if (len + offset >= ramdisk_size) {
        printk(KERN_ERR "You are trying to write past the ramdisk\n");
        return -EAGAIN;
    }


    while (len > 0) {

        list_for_each_entry(curr, ptr, list) {
       /**     pfn = virt_to_pfn(offset); ** STILL DONT HAVE IT FFS **/

            if (page_to_pfn(curr->page) == pfn) {

                remap_pfn_range(vma, vma->vm_start + offset,
                                pfn + (PAGE_SIZE * index), len, vma->vm_page_prot);
                index++;
                len -= PAGE_SIZE;
                offset += PAGE_SIZE;
            }
        }
    }

    // so now get the page number for where we should start
    // loop through every page till we find where we should start 
    // add the page with remap
    // make length = length - page_size
    // move on to the next page

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

    asgn1_device.dev = MKDEV(asgn1_major, 0);
    atomic_set(&asgn1_device.max_nprocs, 1);
    atomic_set(&asgn1_device.nprocs, 0);
    asgn1_device.data_size = 0;

    if (asgn1_major) {
        // try register given major number
        result = register_chrdev_region(asgn1_device.dev, asgn1_dev_count, "Eds_char_device");
        
        if (result < 0) {
            // uh oh didnt work better get system to allocate it
            printk(KERN_WARNING "Can't use the major number %d; trying automatic allocation..\n", asgn1_major);
            
            // check if system can give me a number or die
            if ((result = alloc_chrdev_region(&asgn1_device.dev, asgn1_major, asgn1_dev_count, MYDEV_NAME)) < 0) {
                printk(KERN_ERR "Failed to allocate character device region\n");
                return -1;
            }
            asgn1_major = MAJOR(asgn1_device.dev);
        }
    } 
    else {
        // user hasnt given me a major number so ill make my own
        if ((result = alloc_chrdev_region(&asgn1_device.dev, asgn1_major, asgn1_dev_count, MYDEV_NAME)) < 0) {
            printk(KERN_ERR "Failed to allocate character device region\n");
            return -1;
        }
        asgn1_major = MAJOR(asgn1_device.dev);
    }
    
    // allocate cdev
    if (!(asgn1_device.cdev = cdev_alloc())) {
        printk(KERN_ERR "cdev_alloc() failed.\n");
        unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);
        return -1;
    }
    
    // init cdev
    cdev_init(asgn1_device.cdev, &asgn1_fops);

    asgn1_device.cdev->owner = THIS_MODULE;

    // add cdev
    if (cdev_add(asgn1_device.cdev, asgn1_device.dev, asgn1_dev_count) < 0) {
        printk(KERN_ERR "cdev_add() failed.\n");
        cdev_del(asgn1_device.cdev);
        unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);
        return -1;
    }

    // initiliase page list
    INIT_LIST_HEAD(&(asgn1_device.mem_list));
    
    if (create_proc_read_entry(MYDEV_NAME, S_IRUSR | S_IRGRP | S_IROTH, NULL, asgn1_read_procmem, NULL) == NULL) {
        printk(KERN_ERR "Error: Could not initialize /proc/%s/\n", MYDEV_NAME);
        result = -ENOMEM;
        goto fail_class;
    }

    // create class
    asgn1_device.class = class_create(THIS_MODULE, MYDEV_NAME);
    if (IS_ERR(asgn1_device.class)) {
        printk(KERN_WARNING "%s: can't create udev class\n", MYDEV_NAME);
        result = -ENOMEM;
        goto fail_class;
    }

    // create device
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

// cleanup if class init fails
fail_class:
    remove_proc_entry(MYDEV_NAME, NULL);
    list_del_init(&asgn1_device.mem_list);
    cdev_del(asgn1_device.cdev);
    unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);

    return result;

// cleanup if device init fails
fail_device:
    class_destroy(asgn1_device.class);
    remove_proc_entry(MYDEV_NAME, NULL);
    list_del_init(&asgn1_device.mem_list);
    cdev_del(asgn1_device.cdev);
    unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);

    return result;
}

/**
 * Finalise the module
 */
void __exit asgn1_exit_module(void){
    device_destroy(asgn1_device.class, asgn1_device.dev);
    class_destroy(asgn1_device.class);
    printk(KERN_WARNING "cleaned up udev entry\n");

    remove_proc_entry(MYDEV_NAME, NULL);
    free_memory_pages();
    list_del_init(&asgn1_device.mem_list);
    cdev_del(asgn1_device.cdev);
    unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);
    
    printk(KERN_WARNING "Good bye from %s\n", MYDEV_NAME);
}


module_init(asgn1_init_module);
module_exit(asgn1_exit_module);

