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
//#include <linux/mutex.h> //added by malcolm (maybe unncessary. scull used mutex without it...)
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Malcolm McKellips"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{

    //Linux device drivers ch3 pg. 58
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);

    PDEBUG("open");
    /**
     * TODO: handle open
     */

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
        struct aesd_dev *dev = filp->private_data;
        ssize_t offs_in_found = 0; //will be the offset in the found command that fpos points to 
        struct aesd_buffer_entry *found_entry;
        ssize_t bytes_to_read = 0;

        PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
        /**
         * TODO: handle read
         */

        //Reference: scull main.c
        //return if mutex wait interrupted
        if (mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;

        //Find the entry and offset within that entry corresponding to f_pos

        found_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->circ_buff), *f_pos, &offs_in_found);

        if (found_entry == NULL){
            retval = 0; 
            //we don't have seek capability yet to move to the start of the buffer after a read is finished and we are doing another.
            //So assume that if the reader has gotten here, they have already read the entire file and are trying one more read checking if bytes returned was 0. 
            //reset f_pos according to aesd_socket application's necessary operation. 
            //*f_pos = 0; //When we have seek implemented, or if our char driver had other usecases we could remove this. 
            goto read_end;
        }

        //Read from fpos to end of entry and update fpos
        bytes_to_read = ((found_entry->size) - offs_in_found);
        if (count < bytes_to_read)
            bytes_to_read = count;

        if ( copy_to_user(buf, ((found_entry->buffptr) + offs_in_found), bytes_to_read) ){
            retval = -EFAULT;
            goto read_end;
        }

        *f_pos +=  bytes_to_read;
        retval = bytes_to_read;

    read_end:
        //unlock lock here...
        mutex_unlock(&dev->lock);
        PDEBUG("read returning with %zu bytes read", retval);
        PDEBUG("filepos after read: %lld",*f_pos);
        return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
        ssize_t retval = -ENOMEM;
        struct aesd_dev *dev = filp->private_data;
        char * new_write;
        const char * old_buffer;
        char * full_buffer; //will hold old_buffer + new_write when \n received.
        char * newl_ptr; //Will be a pointer to the newline char in a new write if found
        ssize_t len_cmd; //Length up to and including the newline if a newline recvd
        ssize_t full_buffer_size; //size of full buffer when we are about to write it to circular buffer
        ssize_t i;

        PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
        /**
         * TODO: handle write
         */

        //Reference: scull main.c
        //return if mutex wait interrupted
        if (mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;

        new_write = (char *)kmalloc(count, GFP_KERNEL);
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
        //kfree(dev->current_entry.buffptr);
        //NO!!! Don't free old current entry. it could have been moved into circular buffer.

        //will return pointer to where the newline char is in new_write
        newl_ptr = strchr(new_write, '\n');

        if(newl_ptr){
            //new line recvd, we must write to buffer all values upto and including newline
            len_cmd = (ssize_t)(newl_ptr - new_write); 
            len_cmd += 1; // +1 because 0 indexed (might need another +1 for null terminator. hopefully read takes care of this for us)

            full_buffer_size = len_cmd + dev->current_entry.size; //the new full buffer to save in circ buff contains new write until newline plus old saved chars

            full_buffer = (char *)kmalloc( full_buffer_size, GFP_KERNEL);

            //Copy all bytes from previous writes in current_entry to the full buffer for the circbuff
            if (dev->current_entry.buffptr != NULL){
                for (i = 0 ; i < dev->current_entry.size; i++){
                    full_buffer[i] = dev->current_entry.buffptr[i];
                }
            }

            if (new_write != NULL){
                for (i = 0; i < len_cmd; i++){
                    full_buffer[(dev->current_entry.size) + i] = new_write[i];
                }
            }

            //Now full_buffer is set up correctly.
            //can free old buffptr for current entry and newwrite(it should not be present in the circ buff yet and its contents are now in full ptr)
            kfree(dev->current_entry.buffptr);
            kfree(new_write);

            dev->current_entry.buffptr = full_buffer;
            dev->current_entry.size = full_buffer_size;

            //Add entry and if it replaced something in the circular buffer, free the old full buffer value...
            old_buffer = aesd_circular_buffer_add_entry(&(dev->circ_buff), &(dev->current_entry));
            if (old_buffer)
                kfree(old_buffer);

            //if we are writing to buffer, set current_entry buffer to null so that it won't be double freed in module cleanup
            dev->current_entry.buffptr = NULL;
            dev->current_entry.size = 0; //reset size to 0 for next write
            retval = len_cmd; 
        }
        else{
            //simply append new bytes to current entry
            full_buffer_size = dev->current_entry.size + count;
            full_buffer = (char *)kmalloc( full_buffer_size, GFP_KERNEL);

            //Copy all bytes from previous writes in current_entry to the full buffer for the circbuff
            if (dev->current_entry.buffptr != NULL){
                for (i = 0 ; i < dev->current_entry.size; i++){
                    full_buffer[i] = dev->current_entry.buffptr[i];
                }
            }

            if (new_write != NULL){
                for (i = 0; i < count; i++){
                    full_buffer[(dev->current_entry.size) + i] = new_write[i];
                }
            }

            //Now full_buffer is set up correctly. Can free old buffptr and newwrite as their contents is in full buffer
            kfree(dev->current_entry.buffptr);
            kfree(new_write);

            dev->current_entry.buffptr = full_buffer;
            dev->current_entry.size = full_buffer_size;
            retval = count;
        }

    write_end:
        //unlock lock here...
        mutex_unlock(&dev->lock);
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
    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    uint8_t index;
    struct aesd_buffer_entry *entry;

    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    //Free any dynamically allocated complete writes in device circular buffer
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&(aesd_device.circ_buff),index) {
        kfree(entry->buffptr);
    }

    // //Free any dynamically allocated partial write in current entry
    //WARNING: Introduced a double free bug in the simple case. Need to be more clever about this.
    kfree(aesd_device.current_entry.buffptr);

    //deinitialize lock
    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
