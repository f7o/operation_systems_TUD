#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <asm/uaccess.h>

#include "fifo.h"

MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Lab Solution");
MODULE_LICENSE("GPL");

#define FIFO_MAJOR 240
#define WRITE_END 0
#define READ_END 1

#define WRITE_NAME "fifo0"
#define READ_NAME "fifo1"

// -------- globals ------------------------------------------------------

// proc pointers
struct proc_dir_entry* procfs_fifo = 0;
struct proc_dir_entry* procfs_config = 0;

// test dev
struct fifo_dev dev;

// cdevs
struct cdev* cdev_write;
struct cdev* cdev_read;
struct class* class_write;
struct class* class_read;

// name of the LKM
const char* mod_name = "fifo_mod";

// -------- globals end --------------------------------------------------

// -------- /dev/fifo* ops -----------------------------------------------

static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct fifo_dev* dev = (struct fifo_dev*)file->private_data;
	return fifo_read(dev, buf, count);
}

static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct fifo_dev* dev = (struct fifo_dev*)file->private_data;
	return fifo_write(dev, buf, count);
}

static int fifo_open(struct inode * inode, struct file * filp)
{
	// check for right device
	if (iminor(inode) > READ_END || iminor(inode) < WRITE_END || imajor(inode) != FIFO_MAJOR)
	{
		printk(KERN_INFO "--- %s: fifo_open failed!\n", mod_name);
		return -ENODEV;
	}

	if (dev.used)
	{
		printk(KERN_INFO "--- %s: fifo already in use!\n", mod_name);
		return -EBUSY;
	}

	// let the file point to the fifo queue
	filp->private_data = &dev;

	return 0;
}

// release method for close calls to fifo0 and fifo1
static int fifo_release(struct inode * inode, struct file * file)
{
	return 0;
}

// -------- /proc/fifo_config ops --------------------------------------

static int config_read(struct seq_file* seq, void* v)
{
	seq_printf(seq, "buf size = %lu\ncurrently stored = %lu\ntotal write = %lu\ntotal read = %lu\n",
		dev.size, dev.stored, dev.write_bytes, dev.read_bytes);
	return 0;
}

static ssize_t config_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int err;
	size_t new_size;

	// the local buffer
	char local[5];

	// do nothing if flag is taken
	if (dev.used)
	{
		printk(KERN_INFO "--- %s: no resize while reading or writing!\n", mod_name);
		return -EBUSY;
	}

	// throw away wrong sizes (yes, there seems to be no \0 char hat the end)
	if (count > 4 || count < 1)
	{
		printk(KERN_INFO "--- %s: size not set! error: input size was %lu\n", mod_name, count);
		return -EINVAL;
	}

	// get user data (because kstrtoul_from_user did strange things...)
	err = copy_from_user(local, buf, count);
	if (err)
	{
		printk(KERN_INFO "--- %s: size not set! copy_from_user failed!\n", mod_name);
		return -EFAULT;
	}

	// force \0 at position count
	local[count] = '\0';

	// get the actual int
	err = kstrtoul(local, 0, &new_size);
	if (err)
	{
		printk(KERN_INFO "--- %s: size not set! string was: %s\n", mod_name, local);
		return err;
	}

	// the resize op
	err = fifo_resize(&dev, new_size);

	if(err)
	{
		printk(KERN_INFO "--- %s: size not set! errno: %d\n", mod_name, err);
		return err;
	}

	return count;
}

// -------- /proc/fifo_config ops end ---------------------------------------------

static int config_open(struct inode* inode, struct file* filp)
{
	return single_open(filp, config_read, 0);
}

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

// function for class struct setting the permissions for dev/fifo0
static int write_permission(struct device* dev, struct kobj_uevent_env* env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

// function for class struct setting the permissions for dev/fifo1
static int read_permission(struct device* dev, struct kobj_uevent_env* env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0444);
	return 0;
}

/*
 * destroys the devices
 *
 * @read: node exists := 2, device exists := 1, nothing := 0
 * @write: node exists := 2, device exists := 1, nothing := 0
 *
*/
static void destroy_dev_nodes(int read, int write)
{
	if (2 == write)
	{
		device_destroy(class_write, MKDEV(FIFO_MAJOR, WRITE_END));
		cdev_del(cdev_write);
	}
	else if (1 == write)
		cdev_del(cdev_write);

	if (2 == read)
	{
		device_destroy(class_read, MKDEV(FIFO_MAJOR, READ_END));
		cdev_del(cdev_read);
	}
	else if (1 == read)
		cdev_del(cdev_read);

	if (class_write)
		class_destroy(class_write);

	if (class_read)
		class_destroy(class_read);

	unregister_chrdev_region(MKDEV(FIFO_MAJOR, 0), 2);
}

/*
 * create everything concerning the dev interface
 *
 */
static int create_dev_nodes(void)
{
	int err;
	struct device* device;

	dev_t write_num = MKDEV(FIFO_MAJOR, WRITE_END);
	dev_t read_num = MKDEV(FIFO_MAJOR, READ_END);

	cdev_write = cdev_alloc();
	cdev_read = cdev_alloc();

	if (0 == cdev_write || 0 == cdev_read)
	{
		printk(KERN_INFO "--- %s: cdev_alloc failed!\n", mod_name);
		return ENOMEM;
	}

	// get the write dev class
	class_write = class_create(THIS_MODULE, WRITE_NAME);
	if (IS_ERR(class_write))
	{
		destroy_dev_nodes(0, 0);
		return PTR_ERR(class_write);
	}
	class_write->dev_uevent = write_permission;

	// get the read dev class
	class_read = class_create(THIS_MODULE, READ_NAME);
	if (IS_ERR(class_read))
	{
		destroy_dev_nodes(0, 0);
		return PTR_ERR(class_write);
	}
	class_read->dev_uevent = read_permission;

	// initialize structs
	cdev_init(cdev_write, &fifo_write_fops);
	cdev_init(cdev_read, &fifo_read_fops);

	// adds dev to system, makes it live
	err = cdev_add(cdev_write, write_num, 1);
	if (err)
	{
		destroy_dev_nodes(0, 0);
		return err;
	}
	err = cdev_add(cdev_read, read_num, 1);
	if (err)
	{
		destroy_dev_nodes(0, 0);
		return err;
	}

	// add /dev/fifo0
	device = device_create(class_write, 
							0, 				// parent device
							write_num,
							0, 				// additional data
							WRITE_NAME);
	if (IS_ERR(device))
	{
		destroy_dev_nodes(1, 1);
		return PTR_ERR(device);
	}

	// add /dev/fifo1
	device = device_create(class_read, 
							0, 				// parent device
							read_num,
							0, 				// additional data
							READ_NAME);
	if (IS_ERR(device))
	{
		destroy_dev_nodes(1, 2);
		return PTR_ERR(device);
	}

	return 0;
}

// initialize module (executed when using insmod)
static int __init fifo_mod_init(void)
{
	int err;

	// create the procfs entry
	procfs_config = proc_create(
		"fifo_config", 0666, 0, &config_fops);

	// check for null-pointer
	if (0 == procfs_config) 
	{
		printk(KERN_INFO "--- %s: creation of proc/fifo_config failed!\n", mod_name);
		return -1;
	}

	err = fifo_init(&dev, 1024);
	if (err)
	{
		printk(KERN_INFO "--- %s: fifo_init failed!\n", mod_name);	
		proc_remove(procfs_config);
		return err;
	}
	
	err = create_dev_nodes();
	if (err)
	{
		printk(KERN_INFO "--- %s: cdev_node creation failed!\n", mod_name);	
		proc_remove(procfs_config);
		fifo_destroy(&dev);
		return err;
	}

	printk(KERN_INFO "--- %s: is being loaded.\n", mod_name);
	return 0;
}

// cleanup module (executed when using rmmod)
static void __exit fifo_cleanup(void)
{
	proc_remove(procfs_config);

	fifo_destroy(&dev);
	destroy_dev_nodes(2,2);

	printk(KERN_INFO "--- %s: is being unloaded.\n", mod_name);
}

module_init(fifo_mod_init);
module_exit(fifo_cleanup);