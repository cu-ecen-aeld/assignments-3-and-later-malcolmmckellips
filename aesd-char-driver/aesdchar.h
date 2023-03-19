/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_
#include "aesd-circular-buffer.h"

#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

struct aesd_dev
{
    /**
     * TODO: Add structure(s) and locks needed to complete assignment requirements
     */

    struct aesd_circular_buffer circ_buff;  //entire circular buffer of completed writes (already \n)
    struct aesd_buffer_entry current_entry; //current entry in the circular buffer for write until \n 

    //lock

    struct cdev cdev;     /* Char device structure      */
};

//Reminder on existing structs in aesd-circular-buffer.h:

// struct aesd_buffer_entry
// {
//     /**
//      * A location where the buffer contents in buffptr are stored
//      */
//     const char *buffptr;
//     /**
//      * Number of bytes stored in buffptr
//      */
//     size_t size;
// };

// struct aesd_circular_buffer
// {
//     /**
//      * An array of pointers to memory allocated for the most recent write operations
//      */
//     struct aesd_buffer_entry  entry[AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED];
//     /**
//      * The current location in the entry structure where the next write should
//      * be stored.
//      */
//     uint8_t in_offs;
//     /**
//      * The first location in the entry structure to read from
//      */
//     uint8_t out_offs;
//     /**
//      * set to true when the buffer entry structure is full
//      */
//     bool full;
// };


// extern struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
//             size_t char_offset, size_t *entry_offset_byte_rtn );

// extern void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry);

// extern void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer);


#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
