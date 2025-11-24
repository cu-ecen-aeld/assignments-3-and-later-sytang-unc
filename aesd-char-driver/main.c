/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/list.h>
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Stephen Tang"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

LIST_HEAD(partial);
struct write_node {
    const char *buf;
    size_t size;
    struct list_head lh;
};
size_t partial_size;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_buffer_entry *entry;
    size_t offset, rem;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    if (mutex_lock_interruptible(&aesd_device.mutex) == -EINTR)
        return -ERESTARTSYS;

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.buf, *f_pos, &offset);
    if (!entry) {
        retval = 0;
        goto unlock_aesd_read;
    }

    rem = entry->size - offset;
    if (rem < count)
        count = rem;

    if (copy_to_user(buf, entry->buffptr + offset, count)) {
        printk(KERN_ALERT "Failed copy to user");
        retval = -EAGAIN;
        goto unlock_aesd_read;
    }

    retval = count;
    *f_pos += count;

unlock_aesd_read:
    mutex_unlock(&aesd_device.mutex);

    return retval;
}

static void _do_write(const char *buf, size_t count) {
    struct aesd_buffer_entry entry;
    const char *free_ptr;

    entry = (struct aesd_buffer_entry) {
        .buffptr = buf,
        .size = count
    };

    free_ptr = aesd_circular_buffer_add_entry(&aesd_device.buf, &entry);
    if (free_ptr)
        kfree(free_ptr);

    partial_size = 0;
}

static ssize_t do_write(const char *buf, size_t count) {
    bool end_command;
    struct write_node *wn;
    char *write_buf;
    size_t offset;
    struct list_head *pos, *n;

    end_command = buf[count-1] == '\n';
    if (list_empty(&partial) && end_command) {
        _do_write(buf, count);
        return count;
    }

    wn = (struct write_node*) kmalloc(sizeof(struct write_node), GFP_KERNEL);
    if (!wn)
        return -ENOMEM;
    *wn = (struct write_node) {
        .buf = buf,
        .size = count
    };
    list_add_tail(&wn->lh, &partial);
    partial_size += count;
    if (!end_command)
        return count;
    
    write_buf = (char*) kmalloc(partial_size, GFP_KERNEL);
    if (!write_buf)
        return -ENOMEM;

    offset = 0;
    list_for_each_safe(pos, n, &partial) {
        wn = list_entry(pos, struct write_node, lh);
        strncpy(write_buf + offset, wn->buf, wn->size);
        offset += wn->size;
        list_del(pos);
        kfree(wn->buf);
        kfree(wn);
    }

    _do_write(write_buf, partial_size);

    return count;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    char *kbuf;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    if (mutex_lock_interruptible(&aesd_device.mutex) == -EINTR)
        return -ERESTARTSYS;

    kbuf = (char*) kmalloc(count, GFP_KERNEL);
    if (copy_from_user(kbuf, buf, count)) {
        printk(KERN_ALERT "Failed copy from user");
        retval = -EAGAIN;
        goto unlock_aesd_write;
    }

    retval = do_write(kbuf, count);

unlock_aesd_write:
    mutex_unlock(&aesd_device.mutex);

    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    aesd_circular_buffer_init(&aesd_device.buf);
    mutex_init(&aesd_device.mutex);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    uint8_t i;
    struct aesd_buffer_entry *entry;

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buf,i) {
        if (entry->size)
            kfree(entry->buffptr);
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
