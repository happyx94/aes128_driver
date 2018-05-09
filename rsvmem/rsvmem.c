
/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h>   /* kmalloc() */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/uaccess.h> /* copy_from/to_user */
#include <asm/io.h>      /* virt_to_phys() */

#define RESERVED_SIZE 1048576

MODULE_LICENSE("Dual BSD/GPL");

/* Declaration of memory.c functions */
int memory_open(struct inode *inode, struct file *filp);
int memory_release(struct inode *inode, struct file *filp);
ssize_t memory_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t memory_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
void memory_exit(void);
int memory_init(void);

/* Structure that declares the usual file access functions */
struct file_operations rsvmem_fops =
    {
        read : memory_read,
        write : memory_write,
        open : memory_open,
        release : memory_release
    };

/* Declaration of the init and exit functions */
module_init(memory_init);
module_exit(memory_exit);

/* Global variables of the driver */
/* Major number */
int rsvmem_major = 60;
/* Buffer to store data */
void *prsvmem;
unsigned long phy_addr;
unsigned long bus_addr;

int memory_init(void)
{
    int result;

    /* Registering device */
    result = register_chrdev(rsvmem_major, "rsvmem", &rsvmem_fops);
    if (result < 0)
    {
        printk("rsvmem: Failed to obtain major number %d\n", rsvmem_major);
        return result;
    }

    /* Allocating memory for the buffer */
    prsvmem = kmalloc(RESERVED_SIZE, GFP_DMA);
    if (!prsvmem)
    {
        result = -ENOMEM;
        goto fail;
    }
    phy_addr = (unsigned long)virt_to_phys(prsvmem);
    bus_addr = (unsigned long)virt_to_bus(prsvmem);
    printk("rsvmem: Reserved %dKB at %08lx(bus=%08lx, va=%p)\n",
        RESERVED_SIZE / 1024,  
        phy_addr,
        bus_addr,
        prsvmem);
    memset(prsvmem, (int)'G', RESERVED_SIZE);
    ((char *)prsvmem)[10] = 0;
    printk("rsvmem: First 10 bytes of the buffer is... %s\n", (char *)prsvmem);
    printk("rsvmem: Successfully inserted module\n");
    return 0;
fail:
    memory_exit();
    return result;
}

void memory_exit(void)
{
    /* Freeing the major number */
    unregister_chrdev(rsvmem_major, "rsvmem");
    /* Freeing buffer memory */
    if (prsvmem)
    {
        kfree(prsvmem);
    }
    printk("rsvmem: Removed module\n");
}

int memory_open(struct inode *inode, struct file *filp)
{
    /* Success */
    return 0;
}

int memory_release(struct inode *inode, struct file *filp)
{
    /* Success */
    return 0;
}

ssize_t memory_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    unsigned long size = sizeof(unsigned long);

    if ((unsigned long)count < size)
        return 0;

    /* Transfering data to user space */
    copy_to_user(buf, &phy_addr, size);

    return size;
}

ssize_t memory_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    char c;



    if (count > RESERVED_SIZE)
        return 0;
    if (count == 1)
    {
	if (copy_from_user((void *)&c, buf, 1) != 0)
            return 0;
        printk("rsvmem: write 1 byte %c\n", c);
        memset(prsvmem, c, RESERVED_SIZE);
        return 1;
    }
    printk("rsvmem: Write %d bytes to the buffer\n", (int)count);
    return (ssize_t)(count - copy_from_user(prsvmem, buf, (unsigned long)count));
}
