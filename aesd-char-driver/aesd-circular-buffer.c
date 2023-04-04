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
    uint8_t read_index = buffer->out_offs;
    size_t current_char_offset = 0;
    size_t entry_size = 0;

    if (buffer == NULL) {
        return NULL;
    }
    if (entry_offset_byte_rtn == NULL) {
        return NULL;
    }

    if ((buffer->in_offs == buffer->out_offs) && (buffer->full ==0)) {
        /* Empty buffer*/
        return NULL;
    }

    do {
        entry_size = buffer->entry[read_index].size;
        if ((current_char_offset + entry_size) > char_offset) {
            *entry_offset_byte_rtn = char_offset - current_char_offset;
            return &buffer->entry[read_index];
        }
        current_char_offset += entry_size;
        read_index = (read_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        /* At this point, the condition below indicates the code read all the entries */
        if(read_index == buffer->out_offs) break;
  
    } while(current_char_offset <= char_offset);

    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    if(buffer == NULL) {
        return;
    }

    buffer->entry[buffer->in_offs] = *add_entry;

    /* Increment the write pointer */
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if ((buffer->in_offs == buffer->out_offs) && !buffer->full) {
        /* Buffer full*/
        buffer->full = 1;
    }
    /* Once buffer is full, read pointer will follow the write pointer */
    if (buffer->full == 1) {
        buffer->out_offs = buffer->in_offs; 
    }            
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}

/* Returns the location of the buffer pointer where next write willtake place */
struct aesd_buffer_entry * aesd_circular_buffer_return_full_pointer(struct aesd_circular_buffer *buffer, int * buf_status)
{
    *buf_status  = buffer->full; 
    if(buffer->full == 1) {
        return &buffer->entry[buffer->out_offs];
    }
    return &buffer->entry[buffer->in_offs];
}


/* Returns the total size of the buffer, needed for ftell*/
unsigned int aesd_circular_buffer_return_size(struct aesd_circular_buffer *buffer)
{
    
    uint8_t read_index = buffer->out_offs;    
    size_t entry_size = 0;
    while(1) {
        entry_size += buffer->entry[read_index].size;
        read_index = (read_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        /* At this point, the condition below indicates the code read all the entries */
        if(read_index == buffer->in_offs) break;
    }
    return entry_size;
}

/* Returns the offset of a character*/
int aesd_circular_buffer_return_char_offset(struct aesd_circular_buffer *buffer, 
    size_t member_offset, size_t char_offset, size_t *entry_offset_byte_rtn)
{
    
    uint8_t read_index = buffer->out_offs;    
    size_t entry_size = 0;

    if(member_offset > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
        return -1;
    }
    
    while(1) {
        if(member_offset == 0) {
           entry_size += char_offset;
           break;
        }
        entry_size += buffer->entry[read_index].size;
        --member_offset;
        read_index = (read_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        /* At this point, the condition below indicates the code read all the entries */
        if(read_index == buffer->in_offs) {
            if (member_offset > 0)
                return -1;
        }
        
    }
    *entry_offset_byte_rtn = entry_size;
    return 0;
}


