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
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Surya Kanteti"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
    */

    struct aesd_dev* dev;
    dev = containerof(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

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
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    size_t currentOffset; // Offset in working entry
    size_t bytesToRead;
    struct aesd_dev* dev;
    struct aesd_buffer_entry* currentReadEntry;

    // Initial checks
    if(filp == NULL || buf == NULL || f_pos == NULL)
    {
        retval = -EINVAl;
        PDEBUG("NULL argument to aesd_read");
        goto READ_RET;
    }

    // Fetch the aesd_dev structure
    dev = filp->private_data;

    // First go to location pointed by f_pos and get current entry, current offset
    currentReadEntry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->circularBuffer), *f_pos, &currentOffset);
    if(currentReadEntry == NULL)
    {
        goto READ_RET; // Invalid offset or no more bytes to be read
    }

    // Check how many bytes to read from buffer
    if(currentReadEntry->size >= count)
    {
        bytesToRead = count;
    }
    else
    {
        bytesToRead = currentReadEntry->size;
    }


    // Copy to user space buffer
    if(copy_to_user(buf, currentReadEntry->buffptr[currentOffset], bytesToRead))
    {
        retval = -EFAULT;
        goto READ_RET;
    }

    // Update the value of f_pos and count
    f_pos += f_pos + bytesToRead;
    count -= bytesToRead;
    retval = bytesToRead;

    READ_RET: return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    // Fetch the aesd_dev structure
    dev = filp->private_data;
  
    // Initial checks
    if(filp == NULL || buf == NULL || f_pos == NULL)
    {
        retval = -EINVAL;
        goto WRITE_RET;
    }

    // Allocate memory
    char* kernelBuf = kmalloc(count, GFP_KERNEL);
    if(kernelBuf == NULL)
    {
        goto WRITE_RET; // Memory allocation failed
    }

    // Copy from buffer
    if(copy_from_user(kernelBuf, buf, count))
    {
        retval = -EFAULT;
        goto WRITE_RET;
    }

    // Create new entry using this buffer
    struct aesd_buffer_entry newEntry;
    newEntry.buffptr = kernelBuf;
    newEntry.size = count;

    // Write to the circular buffer, clearing old memory for overwriting
    if(dev->circularBuffer->entry[dev->circularBuffer->in_offs].buffptr != NULL)
    {
        kfree(dev->circularBuffer->entry[dev->circularBuffer->in_offs].buffptr);
        aesd_circular_buffer_add_entry(&(dev->circularBuffer), &newEntry);
    }


    // SURYA: Create a buffer and keep appending to until newline is encountered

    // SURYA: Once newline encountered, assign that buffer to write buffer. Use aesd_circular_buffer_add_entry

    // SURYA: In case of overwriting, free the old memory in kernel. Inside add entry func? But there it says it should handle by caller (here).
    // Check if overwriting here, then free the old memory and directly add new entry.Use AESD_CIRCULAR_BUFFER_FOREACH to free? Maybe not
    


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

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    // SURYA: Just initialize mutex, aesd_dev structure already global
    

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

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    // SURYA: De-Initialize the structure from aesdchar.h 

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
