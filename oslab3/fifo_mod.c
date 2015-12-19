#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>		
#include <linux/fs.h>			// file ops struct
#include <linux/proc_fs.h>		// proc_dir_entry
#include <linux/seq_file.h>		// used for procfs read
#include <linux/kernel.h>		// util functions
#include <linux/cdev.h>			// cdev
#include <linux/device.h>		// device struct in create_dev_node
#include <linux/string.h>		// strlen

#include <asm/uaccess.h>		// user space memory access

#include "fifo.h"

MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Lab Solution");
MODULE_LICENSE("GPL");

#define DEV_NAME "deeds_fifo"

// -------- globals ------------------------------------------------------
// proc pointer for the virtual stats file
struct proc_dir_entry* proc_stats = 0;

// the actual fifo queue
struct fifo_dev fifo;

// the kernel char device struct, used in create_dev_node 
struct cdev k_c_dev;

// ptr to the dev class registered within the kernel
struct class* dev_class;

// the acquired dev number (minor and major), dev_open checks for it
dev_t dev_no;

// name of the LKM
const char* mod_name = "fifo_mod";
// -------- globals end --------------------------------------------------

// -------- exported functions, fifo access ------------------------------

// TODO: export these functions
/*
 *	If space is available, the given input is inserted at fifo.end
 *
 * 	@input: the data item to be insterted
 * 
 * 	returns:
 *		0 on success
 *		see fifo_read
 */
static int put(struct data_item* input)
{
	return fifo_write(&fifo, input);
}

/*
 *	If the fifo is not empty, output will be the item at fifo.front
 * 
 *	returns:
 *		ptr to a data_item on success
 *		ERR_PTR(see fifo_read)
 */
struct data_item* get(void)
{
	return fifo_read(&fifo);
}
// -------- exported functions, fifo access end ------------------------------

// -------- user space access --------------------------------------------
/*
 * checks if the right device trys to access the queue
 *
 * returns:
 *	ENODEV if the opening device node has the wrong major or minor number
 * 	0 on success
 */
static int dev_open(struct inode* inode, struct file* filp)
{
	if (imajor(inode) != MAJOR(dev_no) || iminor(inode) != MINOR(dev_no))
	{
		printk(KERN_INFO "---- %s: dev_open failed, wrong device number(s)!\n", mod_name);
		return ENODEV;
	}

	filp->private_data = &fifo;

	return 0;
}
/*
 * implements user read
 *
 * returns:
 * 	no of read bytes on success
 * 	
 */
static ssize_t dev_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	char* return_str;
	struct data_item* di;
	ssize_t real_count = 0;

	// only one read!
	if (*ppos != 0)		// possible race condition? file.ppos should (TM) be per thread?
		return 0;

	// read from fifo
	di = get();
	if (IS_ERR(di))
		return -PTR_ERR(di);

	return_str = kmalloc(count * sizeof(char), GFP_KERNEL);
	real_count = snprintf(return_str, count, "[%lu][%llu] %s",
							di->qid, di->time, di->msg);

	// free the memory not needed anymore
	free_di(di);

	if (copy_to_user(buf, return_str, real_count))
	{
		printk(KERN_INFO "--- %s: dev_read; copy_to_user failed!\n", mod_name);
		real_count = -EFAULT;
	}

	*ppos = real_count;

	kfree(return_str);
	return real_count;
}

/*
 * implements user write
 *
 * returns:
 * 	count on success
 * 	-EFAULT if copy_from_user failed
 *	-EINVAL if contents of buf are malformed
 */
static ssize_t dev_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	ssize_t ret;
	struct data_item* di;
	char* str = kmalloc((count + 1) * sizeof(char), GFP_KERNEL);

	if (copy_from_user(str, buf, count))
	{
		printk(KERN_INFO "--- %s: dev_write; copy_from_user failed!\n", mod_name);
		ret = -EFAULT;
		goto out;
	}

	// force zero termination
	str[count] = '\0'; 

	// get a data_item struct
	di = alloc_di_str(str);
	if (IS_ERR(di))
	{
		ret =  -PTR_ERR(di);
		goto out;
	}

	// write to fifo
	ret = -put(di);
	if (0 == ret)
		ret = count;

out:
	kfree(str);
	return ret;
}

static int dev_release(struct inode* inode, struct file* filp)
{
	return 0;
}

/* 
 * The file ops for user space fifo access
 */
static struct file_operations dev_fops = {
	.owner =	THIS_MODULE,
	.open =		dev_open,
	.read =		dev_read,
	.write =	dev_write,
	.release =	dev_release,
};
// -------- user space access end ----------------------------------------

// -------- stats --------------------------------------------------------
static int stats_read(struct seq_file* seq, void* v)
{
	seq_printf(seq, "implement me!\n");
	return 0;
}

static int stats_open(struct inode* inode, struct file* filp)
{
	return single_open(filp, stats_read, 0);
}

/* 
 * The file ops for user space stats of the queue
 */
static struct file_operations stat_fops = {
	.owner =	THIS_MODULE,
	.open =		stats_open,
	.read =		seq_read,
	.release =	single_release,
};
// -------- stats end ----------------------------------------------------

/*
 * destroys the device node, unregisters the cdev form the kernel
 *
 * @dev: 	node exists := 3, device exists := 2, 
 			class exists := 1, nothing := 0
 *
*/
static void destroy_dev_node(int dev)
{
	switch (dev)
	{
		case 3:
		{
			device_destroy(dev_class, dev_no);
			cdev_del(&k_c_dev);	
			class_destroy(dev_class);
			break;
		}
		case 2:
		{
			cdev_del(&k_c_dev);	
			class_destroy(dev_class);
			break;
		}
		case 1:
		{
			class_destroy(dev_class);
			break;	
		}
		default: break;
	}

	unregister_chrdev_region(dev_no, 1);
}

// function for class struct setting the permissions for the dev node
static int rw_permission(struct device* dev, struct kobj_uevent_env* env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

/*
 * create the device and it's nodes in /dev
 * this is used for user space communication.
 *
 * returns:
 * 	an error code on failure
 * 	0 on success
 */
static int create_dev_node(void)
{
	int err;
	struct device* device;

	// get a major number
	err = alloc_chrdev_region(&dev_no, 0, 1, DEV_NAME);
	if (err)
		return err;

	// register a dev class
	dev_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(dev_class))
	{
		destroy_dev_node(0);
		return PTR_ERR(dev_class);
	}
	dev_class->dev_uevent = rw_permission;

	// initialize the cdev struct
	cdev_init(&k_c_dev, &dev_fops);

	// adds dev to system, makes it live
	err = cdev_add(&k_c_dev, dev_no, 1);
	if (err)
	{
		destroy_dev_node(1);
		return err;
	}

	// add /dev/fifo
	device = device_create(dev_class, 
							0, 				// parent device
							dev_no,
							0, 				// additional data
							DEV_NAME);
	if (IS_ERR(device))
	{
		destroy_dev_node(2);
		return PTR_ERR(device);
	}
	return 0;
}

/*
 * initializes the LKM
 * calls functions to create/init the following:
 *		the stats file in /proc
 * 		the device node in /dev
 *		the fifo_queue is initialized
 */
static int __init fifo_mod_init(void)
{
	int err;

	err = fifo_init(&fifo, 0);
	if (err)
	{
		printk(KERN_INFO "--- %s: fifo_init failed!\n", mod_name);	
		return err;
	}

	proc_stats = proc_create(
		"deeds_fifo_stats", 0444, 0, &stat_fops);

	if (0 == proc_stats) 
	{
		printk(KERN_INFO "--- %s: creation of proc/deeds_fifo_stats failed!\n", mod_name);
		fifo_destroy(&fifo);
		return -1;
	}
	
	err = create_dev_node();
	if (err)
	{
		printk(KERN_INFO "--- %s: cdev (and node) creation failed!\n", mod_name);	
		proc_remove(proc_stats);
		fifo_destroy(&fifo);
		return err;
	}

	printk(KERN_INFO "--- %s: is being loaded.\n", mod_name);
	return err;
}

static void __exit fifo_mod_cleanup(void)
{
	fifo_destroy(&fifo);

	proc_remove(proc_stats);
	destroy_dev_node(3);

	printk(KERN_INFO "--- %s: is being unloaded.\n", mod_name);
}
module_init(fifo_mod_init);
module_exit(fifo_mod_cleanup);