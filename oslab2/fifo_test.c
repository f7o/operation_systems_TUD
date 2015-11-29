#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include <asm/uaccess.h>

#include "fifo.h"

MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Lab Solution");
MODULE_LICENSE("GPL");

// --- globals ---
// proc pointers
struct proc_dir_entry* procfs_fifo = 0;
struct proc_dir_entry* procfs_config = 0;

// test dev
struct fifo_dev dev;

// cdevs
struct cdev* cdev_write;
struct cdev* cdev_read;
struct class* class;

// name of the LKM
const char* mod_name = "fifo_mod";

// --- globals end ---

// handles read calls to procfs_fifo.
// gets the system time and prints it to procfs_fifo.
// format of buf depends on format_option.
// returns the amount of chars read or 0 if eof is reached 
static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	return fifo_read(&dev, buf, count);
}

static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	return fifo_write(&dev, buf, count);
}

static int config_read(struct seq_file* seq, void* v)
{
	seq_printf(seq, "buf size = %lu\ncurrently stored = %lu\ntotal write = %lu\ntotal read = %lu\n",
		dev.size, dev.stored, dev.write_bytes, dev.read_bytes);
	return 0;
}

static ssize_t config_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int err;

	// the local buffer
	char local[5];

	if (count > 4 || count < 1)
	{
		printk(KERN_INFO "--- %s: size not set! error: input size was %lu\n", mod_name, count);
		return -EINVAL;
	}

	err = copy_from_user(local, buf, count);
	if (err)
	{
		printk(KERN_INFO "--- %s: size not set! copy_from_user failed!\n", mod_name);
		return -EFAULT;
	}

	// force \0 at position count
	local[count] = '\0';

	size_t new_size;
	err = kstrtoul(local, 0, &new_size);
	if (err)
	{
		printk(KERN_INFO "--- %s: size not set! string was: %s\n", mod_name, local);
		return err;
	}

	err = fifo_resize(&dev, new_size);
	if(err)
	{
		printk(KERN_INFO "--- %s: size not set! errno: %d\n", mod_name, err);
		return err;
	}

	return count;
}

static int config_open(struct inode* inode, struct file* filp)
{
	return single_open(filp, config_read, 0);
}

/*
// this method is called whenever the module is being used
// e.g. for both read and write operations
static int time_open(struct inode * inode, struct file * file)
{
	return 0;
}

// this method releases the module and makes it available for new operations
static int time_release(struct inode * inode, struct file * file)
{
	return 0;
}
*/

static struct file_operations fifo_read_fops = {
	.owner =	THIS_MODULE,
	.read =		read,
	.open =		fifo_open,
	.release =	fifo_release,
};

static struct file_operations fifo_write_fops = {
	.owner =	THIS_MODULE,
	.write =	write,
	.open =		fifo_open,
	.release =	fifo_release,
};

static struct file_operations config_fops = {
	.owner =	THIS_MODULE,
	.open =		config_open,
	.read =		seq_read,
	.write =	config_write,
	.llseek = 	seq_lseek,
	.release =	single_release,
};

static int create_dev_nodes(void)
{
	int err;
	struct device* device;

	cdev_write = cdev_alloc();
	cdev_read = cdev_alloc();

	if (0 == cdev_write || 0 == cdev_read)
	{
		printk(KERN_INFO "--- %s: cdev_alloc failed!\n", mod_name);
		return ENOMEM;
	}

	cdev_init(cdev_write, fifo_write_fops);
	cdev_init(cdev_read, fifo_read_fops);

	class = create_class(THIS_MODULE, "fifo");

	dev_t write_num = MKDEV(240, 0);
	dev_t read_num = MKDEV(240, 1);

	// adds dev to system, makes it live
	err = cdev_add(cdev_write, write_num, 1);
	if (err)
		return err;
	err = cdev_add(cdev_read, read_num, 1);
	if (err)
		return err;

	device = device_create(class, 
							0, 				// parent device
							write_num,
							0, 				// additional data
							"fifo0");

	device = device_create(class, 
							0, 				// parent device
							read_num,
							0, 				// additional data
							"fifo1");

	return 0;
}

// initialize module (executed when using insmod)
static int __init time_init(void)
{
	int err;

	// create the procfs entries
	procfs_fifo = proc_create(
		"fifo", 0666, 0, &fifo_fops);
	procfs_config = proc_create(
		"fifo_config", 0666, 0, &config_fops);

	if (0 == procfs_fifo || 0 == procfs_config) 
	{
		printk(KERN_INFO "--- %s: creation of at least one procfs_entry failed!\n", mod_name);
		return -1;
	}

	fifo_init(&dev, 1024);
	
	err = create_dev_nodes();
	if (err)
	{
		printk(KERN_INFO "--- %s: cdev_node creation failed!\n", mod_name);	
		return err;
	}

	// check for null-pointers
	printk(KERN_INFO "--- %s: is being loaded.\n", mod_name);
	return 0;
}

static void destroy_dev_nodes(void)
{
	device_destroy(class, MKDEV(240, 0));
	device_destroy(class, MKDEV(240, 1));
	cdev_del(cdev_write);
	cdev_del(cdev_read);
	class_destroy(class);
}

// cleanup module (executed when using rmmod)
static void __exit time_cleanup(void)
{
	proc_remove(procfs_fifo);
	proc_remove(procfs_config);

	fifo_destroy(&dev);

	printk(KERN_INFO "--- %s: is being unloaded.\n", mod_name);
}

module_init(time_init);
module_exit(time_cleanup);