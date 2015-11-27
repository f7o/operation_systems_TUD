#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>

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

// handles read calls to procfs_config 
static int config_read(struct seq_file* seq, void* v)
{
	seq_printf(seq, "buf size = %lu\ncurrently stored = %lu\ntotal write = %lu\ntotal read = %lu\n",
		dev.size, dev.stored, dev.write_bytes, dev.read_bytes);
	return 0;
}

// handels writes to procfs_config.
// every input beyond the first char is disregarded.
// returns the number of byte written or...
// returns -1 on failure
//		-> buf may only point to '0' or '1'
static ssize_t config_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	if (count > 5 || count < 2)
	{
		printk(KERN_INFO "--- %s: size not set! error: input size.\n", mod_name);
		return -EINVAL;
	}

	size_t new_size;
	int err = kstrtoul(buf, 0, &new_size);
	if (err)
	{
		printk(KERN_INFO "--- %s: size not set! errno: %d\n", mod_name, err);
		return -1;
	}

	err = fifo_resize(&dev, new_size);
	if(err)
	{
		printk(KERN_INFO "--- %s: size not set! errno: %d\n", mod_name, err);
		return -1;
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

static struct file_operations fifo_fops = {
	.owner =	THIS_MODULE,
	.read =		read,
	.write =	write,
//	.open =		time_open,
//	.release =	time_release,
};

static struct file_operations config_fops = {
	.owner =	THIS_MODULE,
	.open =		config_open,
	.read =		seq_read,
	.write =	config_write,
	.llseek = 	seq_lseek,
	.release =	single_release,
};

// initialize module (executed when using insmod)
static int __init time_init(void)
{
	// create the procfs entries
	procfs_fifo = proc_create(
		"fifo", 0666, 0, &fifo_fops);
	procfs_config = proc_create(
		"fifo_config", 0666, 0, &config_fops);

	fifo_init(&dev, 1024);

	// check for null-pointers
	if (0 == procfs_fifo || 0 == procfs_config) 
	{
		printk(KERN_INFO "--- %s creation of at least one procfs_entry failed!\n", mod_name);
		return -1;
	}
	printk(KERN_INFO "--- %s is being loaded.\n", mod_name);
	return 0;
}

// cleanup module (executed when using rmmod)
static void __exit time_cleanup(void)
{
	proc_remove(procfs_fifo);
	proc_remove(procfs_config);

	fifo_destroy(&dev);

	printk(KERN_INFO "--- %s is being unloaded.\n", mod_name);
}

module_init(time_init);
module_exit(time_cleanup);