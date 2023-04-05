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
 * CREDIT: https://sites.google.com/site/linuxkernel88/sample-code/writing-a-character-driver
 *         Consulted above link for llseek implementation
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
#include "aesd-circular-buffer.h"
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include "aesd_ioctl.h"

/*******************************************************************************
 * Definitions
*******************************************************************************/

/*******************************************************************************
 * Prototypes
*******************************************************************************/
static void * kernel_mem_realloc(void* old_mem, size_t old_len,
                   size_t new_len, unsigned int mode);
static int aesd_open(struct inode *inode, struct file *filp);
static int aesd_release(struct inode *inode, struct file *filp);
static ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos);
static ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos);

static loff_t aesd_llseek(struct file *file, loff_t offset, int whence);

static long aesd_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

static int aesd_setup_cdev(struct aesd_dev *dev);
static int aesd_init_module(void);
static void aesd_cleanup_module(void);



/*******************************************************************************
 * Variables and Macros
*******************************************************************************/

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

static int broken_packet = 0;
static char *temp_dev_buff = NULL;
static size_t write_cnt = 0;
static struct aesd_circular_buffer aesd_buf;
static struct aesd_dev aesd_device;

struct file_operations aesd_fops = {
    .owner          =   THIS_MODULE,
    .read           =   aesd_read,
    .write          =   aesd_write,
    .open           =   aesd_open,
    .release        =   aesd_release,
    .llseek         =   aesd_llseek,
    .unlocked_ioctl =   aesd_ioctl,
};


/*******************************************************************************
 * Code
*******************************************************************************/


static void * kernel_mem_realloc(void* old_mem, size_t old_len,
                   size_t new_len, unsigned int mode)
{
    void *new_mem;
    new_mem = kmalloc(new_len, mode);
    if (new_mem == NULL) {
        printk(KERN_ERR "aesdchar: kernel_mem_realloc allocation error %d \n", __LINE__);
        return NULL;
    }
    else {
        memcpy(new_mem, old_mem, old_len);
        kfree(old_mem);
    }

    return new_mem;
}

static int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev; /* device information */
    PDEBUG("open \n");    
	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev; /* for other methods */
    return 0;
}

static int aesd_release(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev; /* device information */
    int ret = 0;
    PDEBUG("release \n");
    dev = (struct aesd_dev *)filp->private_data;
	if (dev == NULL)
		ret = -ENODEV;    
    return ret;
}

static ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    unsigned long copy_size;    
    ssize_t rc = 0;
    struct aesd_buffer_entry *dev_buf;
    size_t entry_offset_byte = 0;
    int ret = 0;
    int size = 0;
    int byte_can_be_sent = 0;

    struct aesd_dev *dev_struct = (struct aesd_dev *)filp->private_data;

    PDEBUG("read %zu bytes with offset %lld \n",count,*f_pos);
    
    printk("aesd_read:read %zu bytes with offset %lld \n",count,*f_pos);
 
    ret = mutex_lock_interruptible(&dev_struct->aesdchar_mutex);
    if (ret < 0) {
        printk(KERN_ERR"Mutex lock failed \n");
        return ret;
    }

    size = aesd_circular_buffer_return_size(dev_struct->aesd_buffer);
    if (*f_pos >= size) {
        PDEBUG ("End of file reached \n");
        rc = 0;
        goto return_no_mem;
    }

    dev_buf = aesd_circular_buffer_find_entry_offset_for_fpos(dev_struct->aesd_buffer, 
                    *f_pos, &entry_offset_byte);

    PDEBUG("aesdchar: aesd_read: f_pos = %lld, offset = %d \n", *f_pos, entry_offset_byte);

    if(dev_buf == NULL) {
        PDEBUG ("Buffer is NULL, returning \n");
        rc = 0;
        goto return_no_mem;
    }

    byte_can_be_sent = dev_buf->size - entry_offset_byte;
    if (byte_can_be_sent >= count)
        byte_can_be_sent = count;
    
    copy_size = copy_to_user(buf, dev_buf->buffptr + entry_offset_byte, byte_can_be_sent);
    if ( copy_size != 0) {        
        PDEBUG("Copy_to_user error \n");
        rc = -EFAULT;
        goto err_cleanup;
    }

    *f_pos += byte_can_be_sent;
    
    err_cleanup:
    if (rc == 0) {
        rc = byte_can_be_sent;
    }
    return_no_mem:
	mutex_unlock(&dev_struct->aesdchar_mutex);

    return rc;
}


static ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{    
    unsigned long copy_size;
    ssize_t rc = 0;
    int ret = 0;
    char *buf_ptr = NULL;
    struct aesd_buffer_entry *aesd_buf_entry;
    char *nl_index = NULL;    
    int status = 0;    
    char * device_buffer = NULL;
    struct aesd_dev *dev_struct =  (struct aesd_dev *)filp->private_data;

    PDEBUG("write %zu bytes with offset %lld \n",count,*f_pos);    

       
    device_buffer = (char *)kmalloc(sizeof(char) * count, GFP_KERNEL);
    if (device_buffer == NULL) {
        PDEBUG("aesdchar: Memory allocation error %d \n", __LINE__);
        rc = -ENOMEM;
        goto err_return_no_lock;
    }

    ret = mutex_lock_interruptible(&dev_struct->aesdchar_mutex);
    if (ret < 0) {
        printk(KERN_ERR"Mutex lock failed \n");
        return ret;
    }
    
    copy_size = copy_from_user(device_buffer, buf, count);
    if ( copy_size != 0) {        
        PDEBUG("Copy_from_user error \n");
        rc = -EFAULT;
        goto err_mem_clean_0;
    }

    nl_index = memchr(device_buffer, '\n', count);
    if (nl_index) {
        if(broken_packet == 1) {
            buf_ptr = kernel_mem_realloc(temp_dev_buff, write_cnt, write_cnt+count, GFP_KERNEL);
            if(buf_ptr == NULL) {                
                PDEBUG("aesdchar: kernel_mem_realloc allocation error %d \n", __LINE__);
                kfree(temp_dev_buff);                
                rc = -ENOMEM;                
                goto err_mem_clean_0;
            }
            temp_dev_buff = buf_ptr;
            memcpy((void *)&temp_dev_buff[write_cnt], device_buffer, count);
            write_cnt += count; 
        }

        /* Check if buffer is full */
        aesd_buf_entry = aesd_circular_buffer_return_full_pointer(dev_struct->aesd_buffer, &status);
        if(status == 1) {
            kfree(aesd_buf_entry->buffptr);
            aesd_buf_entry->size = 0;
            status = 0;
        }
        
        aesd_buf_entry->buffptr = (broken_packet == 1) ? temp_dev_buff : device_buffer;
        aesd_buf_entry->size = (broken_packet == 1) ? write_cnt : count;
        aesd_circular_buffer_add_entry(dev_struct->aesd_buffer, aesd_buf_entry);
        broken_packet = 0;
        write_cnt = 0;
        temp_dev_buff = NULL;
        
        
    } 
    else {

        if(broken_packet == 0) {
            temp_dev_buff = kmalloc(sizeof(char) * count, GFP_KERNEL);
            if(temp_dev_buff == NULL) {                
                PDEBUG("aesdchar: kmallo failed at %d \n", __LINE__);              
                rc = -ENOMEM;                
                goto err_mem_clean_0;
            }        
            memcpy(temp_dev_buff,device_buffer,count);
            broken_packet = 1;
        }
        else {
            buf_ptr = kernel_mem_realloc(temp_dev_buff, write_cnt, write_cnt+count, GFP_KERNEL);
            if(buf_ptr == NULL) {                
                PDEBUG("aesdchar: kernel_mem_realloc allocation error %d \n", __LINE__);
                if(temp_dev_buff != NULL) {
                    kfree(temp_dev_buff);   
                }
                rc = -ENOMEM;                
                goto err_mem_clean_0;
            }
            temp_dev_buff = buf_ptr;
            memcpy((void *)&temp_dev_buff[write_cnt], device_buffer, count);
        }
        write_cnt += count;        
        kfree(device_buffer);
    }
    goto err_return;
    err_mem_clean_0:
    kfree(device_buffer);
    err_return:
    if(rc == 0) {
        rc = count;
        *f_pos += count;
    }
    mutex_unlock(&dev_struct->aesdchar_mutex);
    err_return_no_lock:
    return rc;
}


static loff_t aesd_llseek(struct file *file, loff_t offset, int whence)
{
    loff_t new_buf_position = 0;
    unsigned int device_size = 0;
    int ret = 0;
    struct aesd_dev *dev_struct = (struct aesd_dev *)file->private_data;
    
    ret = mutex_lock_interruptible(&dev_struct->aesdchar_mutex);
    if (ret < 0) {
        printk(KERN_ERR"Mutex lock failed \n");
        return ret;
    }
    
    device_size  = aesd_circular_buffer_return_size(dev_struct->aesd_buffer);
    switch (whence) {
    case SEEK_SET:
        new_buf_position = offset;
        break;

    case SEEK_CUR:
        new_buf_position = file->f_pos + offset;
        break;

    case SEEK_END:
        new_buf_position = device_size - offset;
        break;

    }

    if (new_buf_position > device_size) {
        return -EINVAL;
    }

    if( new_buf_position < 0 ) new_buf_position = 0;

    PDEBUG("aesdchar: aesd_llseek: f_pos = %lld, offset = %lld new_buf_position =%lld \n", file->f_pos, offset, new_buf_position);
    file->f_pos = new_buf_position;

    mutex_unlock(&dev_struct->aesdchar_mutex);

    return new_buf_position;
}

static long aesd_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct aesd_seekto st;
    long err = 0;
    size_t entry_offset_byte_rtn = 0;
    struct aesd_dev *dev_struct = (struct aesd_dev *)filep->private_data;
    PDEBUG("aesd_ioctl \n");
    memset(&st, 0x0, sizeof(struct aesd_seekto));
    err = mutex_lock_interruptible(&dev_struct->aesdchar_mutex);
    if (err < 0) {
        printk(KERN_ERR"Mutex lock failed \n");
        return err;
    }

    switch (cmd) {
        case AESDCHAR_IOCSEEKTO:
            if(copy_from_user(&st, (char*)arg, sizeof(struct aesd_seekto))) {
                printk(KERN_ERR "Failed to copy data from user space\n");
                err = -EFAULT;
                goto err_return;
            }
            if(aesd_circular_buffer_return_char_offset(dev_struct->aesd_buffer, 
            st.write_cmd, st.write_cmd_offset, &entry_offset_byte_rtn)) {
                err = -EINVAL;
                goto err_return;
            }
            filep->f_pos = entry_offset_byte_rtn;
            break;

        default:
            err = -ENOTTY;
    }

err_return:
    mutex_unlock(&dev_struct->aesdchar_mutex);
    PDEBUG("aesd_ioctl: fops = %d \n", filep->f_pos);
    return err;

}
                
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


static int aesd_init_module(void)
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

    /*
    ** TODO: initialize the AESD specific portion of the device
    **
    */
    
    mutex_init(&aesd_device.aesdchar_mutex);

    aesd_circular_buffer_init(&aesd_buf);
    aesd_device.aesd_buffer = &aesd_buf;
    
    
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }

    PDEBUG("AESDChar driver load major number = %d \n", aesd_major);
    return result;

}

static void aesd_cleanup_module(void)
{
    uint8_t index = 0;
    struct aesd_buffer_entry *entry;

    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /*
    ** TODO: cleanup AESD specific poritions here as necessary
    **
    */
    PDEBUG("Freeing memory from clean-up module\n");
    mutex_destroy(&aesd_device.aesdchar_mutex);
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_buf,index) {
        kfree(entry->buffptr);
    }
    unregister_chrdev_region(devno, 1);

}


MODULE_AUTHOR("Sujoy Ray"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");
module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

