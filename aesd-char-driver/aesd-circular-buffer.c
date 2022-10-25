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
    if(buffer != NULL && entry_offset_byte_rtn != NULL)
    {
        uint8_t current_offs = buffer->out_offs;

        // Loop till we can find the char_offset
        while(char_offset >= buffer->entry[current_offs].size)
        {
            char_offset -= buffer->entry[current_offs].size;

            // Update current location
            if(current_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
                current_offs = 0;
            else
                current_offs += 1;

            // If back at the same location, return NULL
            if(current_offs == buffer->out_offs)
                return NULL;
        }

        // Reached the offset
        *entry_offset_byte_rtn = char_offset;
        return &(buffer->entry[current_offs]);
    }
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
char* aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    char* currentBuf = NULL;
    if(buffer != NULL && add_entry != NULL)
    {
        // Update out_offs if buffer already full    
        if(buffer->full)
        {
            currentBuf = buffer->entry[buffer->out_offs].buffptr;

            if(buffer->out_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
                buffer->out_offs = 0;
            else
                buffer->out_offs++;
        }

        buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
        buffer->entry[buffer->in_offs].size = add_entry->size; // Assign the size as well

        // Update in_offs
        if(buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
            buffer->in_offs = 0;
        else
            buffer->in_offs++;

        // Check if now full
        if(buffer->in_offs == buffer->out_offs)
            buffer->full = true;
    }
    return currentBuf;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
