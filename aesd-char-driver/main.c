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
#include <linux/errno.h> //added by malcolm
#include <linux/string.h> //added by malcolm
#include <linux/slab.h>  //added by malcolm
#include <linux/uaccess.h> //added by malcolm
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Malcolm McKellips"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */

    //Linux device drivers ch3 pg. 58
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);

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
        /**
         * TODO: handle read
         */

        struct aesd_dev *dev = filp->private_data;

        //Find the entry and offset within that entry corresponding to f_pos
        ssize_t offs_in_found = 0; //will be the offset in the found command that fpos points to 
        struct aesd_buffer_entry *found_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->circ_buff), *f_pos, &offs_in_found);

        if (found_entry == NULL){
            retval = 0; 
            goto read_end;
        }

        //Read from fpos to end of entry and update fpos
        ssize_t bytes_to_read = ((found_entry->size) - offs_in_found);
        if ( copy_to_user(buf, ((found_entry->buffptr) + offs_in_found), bytes_to_read) ){
            retval = -EFAULT;
            goto read_end;
        }

        *f_pos +=  bytes_to_read;
        retval = bytes_to_read;

    read_end:
        //unlock lock here...
        return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
        ssize_t retval = -ENOMEM;
        PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
        /**
         * TODO: handle write
         */
        struct aesd_dev *dev = filp->private_data;


        char * new_write = (char *)kmalloc(count, GFP_KERNEL);
        if (!new_write){
            retval = -ENOMEM;
            kfree(new_write);
            goto write_end;
        }


        if (copy_from_user(new_write, buf, count)){
            retval = -EFAULT;
            kfree(new_write);
            goto write_end;
        }

        //This is where the more complicated kmalloc/write logic will go...
        //For now, simple implementation assuming that every write is a complete write with a \n at the end.

        //Free old current entry and set current entry to our new current entry
        kfree(dev->current_entry.buffptr);
        dev->current_entry.buffptr = new_write;
        dev->current_entry.size = count; //these will become a different value when we get more complicated
        retval = count;

        /*
        if(strchr(new_write, '\n')){

        }
        */

        //Add entry and if it replaced something in the circular buffer, free the old value...
        const char * old_buffer = aesd_circular_buffer_add_entry(&(dev->circ_buff), &(dev->current_entry));
        if (old_buffer)
            kfree(old_buffer);

    write_end:
        //unlock lock here...
        //might want a kfree of new_write here in case failure occurs, do not allow memory leak
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
    
    //These three steps are actually all done via the memset above which zero's out entire device struct....
    //aesd_circular_buffer_init(aesd_device.circ_buff); //maybe unnecessary
    //aesd_device.current_entry.buffptr = (const char *)(0);
    //aesd_device.current_entry.size = 0;

    //initialize the lock

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

    //Free any dynamically allocated complete writes in device circular buffer
    uint8_t index;
    struct aesd_buffer_entry *entry;
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&(aesd_device.circ_buff),index) {
        kfree(entry->buffptr);

    //Free any dynamically allocated partial write in current entry
    kfree(aesd_device.current_entry.buffptr);

    //deinitialize lock

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
