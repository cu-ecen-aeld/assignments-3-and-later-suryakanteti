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
#include "aesdchar.h"

#include <linux/slab.h>

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Surya Kanteti"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev* dev;

    PDEBUG("open");
    
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    
    // De-allocate anything that open() allocated in filp->private_data
    filp->private_data = NULL;

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    size_t currentOffset; // Offset in working entry
    size_t bytesToRead;
    struct aesd_dev* dev;
    struct aesd_buffer_entry* currentReadEntry;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    PDEBUG("Proceed to reading!");

    // Initial checks
    if(filp == NULL || buf == NULL || f_pos == NULL)
    {
        retval = -EINVAL;
        goto READ_RET;
    }

    // Fetch the aesd_dev structure
    dev = filp->private_data;

    // Lock the data using a mutex
    if(mutex_lock_interruptible(&(dev->mut)))
    {
        retval = -EINTR;
        goto READ_RET;
    }

    // First go to location pointed by f_pos and get current entry, current offset
    currentReadEntry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->circularBuffer), *f_pos, &currentOffset);
    if(currentReadEntry == NULL)
    {
        goto MUTEX_UNLOCK; // Invalid offset or no more bytes to be read
    }

    // Check how many bytes to read from buffer
    if(currentReadEntry->size - currentOffset >= count)
    {
        bytesToRead = count;
    }
    else
    {
        bytesToRead = currentReadEntry->size - currentOffset;
    }


    // Copy to user space buffer
    if(copy_to_user(buf, currentReadEntry->buffptr + currentOffset, bytesToRead))
    {
        retval = -EFAULT;
        goto MUTEX_UNLOCK;
    }

    // Update the value of f_pos
    *f_pos += bytesToRead;
    retval = bytesToRead;

    MUTEX_UNLOCK: mutex_unlock(&(dev->mut));
    READ_RET: return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev* dev;
    char* tempPtr;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    PDEBUG("Proceed to writing!");

    // Initial checks
    if(filp == NULL || buf == NULL || f_pos == NULL)
    {
        retval = -EINVAL;
        goto WRITE_RET;
    }

    // Fetch the aesd_dev structure
    dev = filp->private_data;

    // Lock the data using a mutex
    if(mutex_lock_interruptible(&(dev->mut)))
    {
        retval = -EINTR;
        goto WRITE_RET;
    }

    // Allocate memory for buffer if required
    if(dev->newWriteEntry.buffptr == NULL)
    {
        dev->newWriteEntry.buffptr = kmalloc(count, GFP_KERNEL);
        if(dev->newWriteEntry.buffptr == NULL)
        {
            goto MUTEX_UNLOCK; // Memory allocation failed
        }
    }
    else
    {
        // Re-allocate the buffer
        tempPtr = krealloc(dev->newWriteEntry.buffptr, dev->newWriteEntry.size + count, GFP_KERNEL);
        if(tempPtr == NULL)
        {
            kfree(dev->newWriteEntry.buffptr);
            goto MUTEX_UNLOCK; // Memory allocation failed
        }
        else
        {
            dev->newWriteEntry.buffptr = tempPtr;
        }
    }

    // Copy the contents and update buffer size
    if(copy_from_user((void*)(dev->newWriteEntry.buffptr + dev->newWriteEntry.size), buf, count))
    {
        retval = -EFAULT;
        goto MUTEX_UNLOCK;
    }
    dev->newWriteEntry.size += count;

    // Write to the circular buffer if newline is encountered
    if(strchr(dev->newWriteEntry.buffptr, '\n') != NULL)
    {
        if(dev->circularBuffer.full) // Free existing memory
        {
            kfree(dev->circularBuffer.entry[dev->circularBuffer.in_offs].buffptr);
        }
        aesd_circular_buffer_add_entry(&(dev->circularBuffer), &(dev->newWriteEntry));

        dev->newWriteEntry.buffptr = NULL;
        dev->newWriteEntry.size = 0;
    }

    retval = count;

    MUTEX_UNLOCK: mutex_unlock(&(dev->mut));
    WRITE_RET: return retval;
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

    // Initialize the mutex
    mutex_init(&(aesd_device.mut));

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    // Clear up the mutex
    mutex_destroy(&(aesd_device.mut));

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
