/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    struct aesd_buffer_entry * circ_buff = buffer->entry;

    size_t offset_left = char_offset; //We can keep track of how much is left to travel, decrementing for each string iterated.
    uint8_t cur_offs = buffer->out_offs; //to move through buffer from out_offs->in_offs

    //When buffer is full, we have to deal with the first entry before traversing because in_offs==out_offs
    if (buffer->full){
        if (offset_left > circ_buff[cur_offs].size-1){
            offset_left -= circ_buff[cur_offs].size;
        }
        else{
            //We are in the correct entry. Set necessary values.
            *entry_offset_byte_rtn = offset_left;
            return &(circ_buff[cur_offs]);
        }
        cur_offs = (cur_offs+1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    while(cur_offs != buffer->in_offs){
        
        if (offset_left > circ_buff[cur_offs].size-1){
            offset_left -= circ_buff[cur_offs].size;
        }
        else{
            //We are in the correct entry. Set necessary values.
            *entry_offset_byte_rtn = offset_left;
            return &(circ_buff[cur_offs]);
        }

        cur_offs = (cur_offs+1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }


    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
* return null or if an existing entry was replaced, return pointer to buffer of entry that was replaced for freeing by caller
*/
const char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{

    struct aesd_buffer_entry * circ_buff = buffer->entry;

    const char *old_entry_buffer = circ_buff[buffer->in_offs].buffptr;

    circ_buff[buffer->in_offs] = *add_entry;

    if (buffer->full){
        //Buffer is full, return old value. In full case that old value was next to be consumed so also update out_ffs
        buffer->in_offs =  (buffer->in_offs+1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        buffer->out_offs = (buffer->out_offs+1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        return old_entry_buffer;
    }
    else{
        //Standard case when not yet full: simply add at the in_offs and advance, remember to update full var.
        buffer->in_offs = (buffer->in_offs+1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; 
        if (buffer->in_offs == buffer->out_offs)
            buffer->full = true;
    }

    return NULL;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
