#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Lab Solution");
MODULE_LICENSE("GPL");

// the max string size used
#define OUTSTR_SIZE 128
// the device numbers
#define DEVICEMAJOR 240
#define WRITEMINOR 0
#define READMINOR 1

// --- globals ---
// name of the LKM
const char* mod_name = "fifo_mod";
// procfs pointer (the config file)
struct proc_dir_entry* fifo_config = 0;

// device info
size_t fifo_size = 128;
size_t fifo_stored = 0;
size_t read_bytes = 0;
size_t write_bytes = 0;

// internals
size_t fifo_new_size = 128;

// position of the first char to be read
size_t fifo_front = 0;
// position of the first empty spot after the last element
size_t fifo_end = 0;

char* buffer = 0;

// --- gloabals end ---

// just a little helper for copying data to the user 
static ssize_t send_data(char* to, const char* from, 
						int offset, int length, int count)
{
	int bytes_to_copy = 1; 
	// only copy the requested amount of data
	if (count < length - offset)
		bytes_to_copy = count;
	else
		bytes_to_copy = length - offset;

	// move string to user space
	if (copy_to_user(to, from + offset, bytes_to_copy))
	{
		printk(KERN_INFO "--- %s: (read) copy failed!\n", mod_name);
		return -1;	// which errno should be used here?
	}
	return bytes_to_copy;
}

// this method is executed when reading from the module
static ssize_t fifo_conf_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	// is there a better solution than this? (thread safe)
	static bool complete = false;
	// keep track of smaller reads (calling this function multiple times)
	static int bytes_read = 0;

	if (complete || 0 == count)
	{	// return with 0 read chars if complete (eof reached)
		// setup further reads
		complete = false; 	
		bytes_read = 0;
		return 0;
	}

	char out_str[OUTSTR_SIZE];
	int length = 0;
	length = snprintf(out_str, OUTSTR_SIZE,
		"buf size = %lu\ncurrently stored = %lu\ntotal write = %lu\ntotal read = %lu\n",
		fifo_size, fifo_stored, write_bytes, read_bytes);
	
	int success_count = send_data(buf, out_str, bytes_read, length, count);
	// check for failed data copy
	if (-1 == success_count)
		return -1;

	// the new offset
	bytes_read += success_count;

	// reading completes if all bytes are read
	if (bytes_read >= length)
		complete = true;

	return success_count;
}

// this method is executed when writing to the module
static ssize_t fifo_conf_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	if (count > 4 || count < 1)
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

	if (new_size < 4097 && new_size > 3 && new_size >= fifo_stored)
		fifo_new_size = new_size;
	else
	{
		printk(KERN_INFO "--- %s: buffer size not changed!\n", mod_name);
		return -EINVAL;
	}

	return count;
}

/*
static ssize_t send_data_wrap(char* to, const char* from, 
							int offset, int length, int count)
{
	
}
*/

static ssize_t fifo_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int success_count = 0;
	if ((fifo_front + count) < fifo_size || (fifo_front + fifo_stored) < fifo_size)
	{
		success_count = send_data(buf, buffer, fifo_front, fifo_stored, count);
		fifo_front += success_count;
	}	
	else
	{
		
	}

	return success_count;
}

static ssize_t fifo_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static void resize(void)
{
	char* tmp = kmalloc(fifo_new_size, GFP_KERNEL);

	size_t left = fifo_stored;

	while (left--)
	{
		*tmp++ = *(buffer + fifo_front);
		fifo_front = (fifo_front +1)%fifo_size; 
	}

	// set the right internals
	fifo_end = fifo_stored;
	fifo_front = 0;
	fifo_size = fifo_new_size;

	//cleanup and buffer change
	kfree(buffer);
	buffer = tmp;
}

// this method is called whenever the module is being used
// e.g. for both read and write operations
static int fifo_open(struct inode * inode, struct file * file)
{
	if (iminor(inode) > 1 || iminor(inode) < 0 || imajor(inode) != DEVICEMAJOR)
	{
		printk(KERN_INFO "--- %s fifo_open failed!\n", mod_name);
		return -ENODEV;
	}
	
	/*
	// get a pointer to the device
	struct cdev* dev;
	dev = container_of(inode->i_cdev, struct cdev, cdev);
	// store the pointer in the file struct (passed to read and write)
	*/
	file->private_data = buffer;

	if (fifo_size != fifo_new_size)
		resize();

	if (0 == buffer)
		buffer = kmalloc(fifo_size, GFP_KERNEL);

	return 0;
}

// this method releases the module and makes it available for new operations
static int fifo_release(struct inode * inode, struct file * file)
{
	return 0;
}


// module's file operations, a module may need more of these
static struct file_operations fifo_fops = {
	.owner =	THIS_MODULE,
	.read =		fifo_read,
	.write =	fifo_write,
	.open =		fifo_open,
	.release =	fifo_release,
};

static struct file_operations config_fops = {
	.owner =	THIS_MODULE,
	.read =		fifo_conf_read,
	.write =	fifo_conf_write,
};


// initialize module (executed when using insmod)
static int __init fifo_mod_init(void)
{
	// create the config proc entry
	fifo_config = proc_create(
		"fifo_config", 0666, 0, &config_fops);
	if (0 == fifo_config)
	{
		printk(KERN_INFO "--- %s creation of proc fifo_config failed!\n", mod_name);
		return -1;
	}

	// register the device, nodes have to be created manually
	if (__register_chrdev(DEVICEMAJOR, 0, 2, "fifo", &fifo_fops) != DEVICEMAJOR)
	{
		printk(KERN_INFO "--- %s registration of chrdev failed!\n", mod_name);
		return -1;
	}	

	printk(KERN_INFO "--- %s is being loaded.\n", mod_name);
	return 0;
}

// cleanup module (executed when using rmmod)
static void __exit fifo_mod_cleanup(void)
{
	// clean up everything done in fifo_mod_init
	__unregister_chrdev(DEVICEMAJOR, 0, 2, "fifo");
	proc_remove(fifo_config);

	// free the buffer
	if (buffer != 0)
		kfree(buffer);

	printk(KERN_INFO "--- %s is being unloaded.\n", mod_name);
}

module_init(fifo_mod_init);
module_exit(fifo_mod_cleanup);