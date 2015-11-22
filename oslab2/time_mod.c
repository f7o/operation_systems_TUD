#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <asm/uaccess.h>

MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Lab Solution");
MODULE_LICENSE("GPL");

// max number of chars to print the time
#define PROCFS_SIZE 64

// --- globals ---
// proc pointers
struct proc_dir_entry* procfs_time = 0;
struct proc_dir_entry* procfs_config = 0;

// name of the LKM
const char* mod_name = "time_mod";

// saved config state
// 0 for sec and 1 for yyyy-mm-dd hh:MM:sec
char format_option = 0;

static ssize_t generate_pretty_time(char* buf, time_t sec)
{
	struct tm time;

	time_to_tm(sec, 0, &time);

	return snprintf(buf, PROCFS_SIZE, "current time: %ld-%d-%d %d:%d:%d%c", 
		time.tm_year + 1900, time.tm_mon +1, time.tm_mday, 
		time.tm_hour, time.tm_min, time.tm_sec, '\0');
}

// handles read calls to procfs_time.
// gets the system time and prints it to procfs_time.
// format of buf depends on format_option.
// returns the amount of chars read or 0 if eof is reached 
static ssize_t time_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	// is there a better solution than this? (thread safe)
	static bool complete = false;

	if (complete)
	{	// return with 0 read chars if complete (eof reached)
		printk(KERN_INFO "--- %s: (time) reading complete!\n", mod_name);		
		complete = false; 	// setup further reads
		return 0;
	}

	if (count < PROCFS_SIZE)
	{
		printk(KERN_INFO "--- %s: (time) buffer too small!\n", mod_name);	
		return -EINVAL;
	}	

	// reading completes after one run
	complete = true;

	printk(KERN_INFO "--- %s: (time) reading... \n", mod_name);

	// get the time
	struct timeval time;
	do_gettimeofday(&time);

	// make a string
	char out_str[PROCFS_SIZE];
	int length = 0;
	if (0 == format_option)
		length = snprintf(out_str, PROCFS_SIZE, "current time: %ld seconds%c", time.tv_sec, '\0');
	else if (1 == format_option)
		length = generate_pretty_time(out_str, time.tv_sec);

	printk(KERN_INFO "--- %s: (time) trying to copy '%s'\n", mod_name, out_str);
	if (copy_to_user(buf, out_str, length))
	{
		printk(KERN_INFO "--- %s: (time) copy failed!\n", mod_name);
		return -1;	// which errno should be used here?
	}

	return length;
}

// handles read calls to procfs_config
// returns the amount of chars read or 0 if eof is reached 
static ssize_t config_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	static bool complete = false;

	if (complete)
	{	// return with 0 read chars if complete (eof reached)
		printk(KERN_INFO "--- %s: (config) reading complete!\n", mod_name);		
		complete = false; 	// setup further reads
		return 0;
	}

	if (count < PROCFS_SIZE)
	{
		printk(KERN_INFO "--- %s: (config) buffer too small!\n", mod_name);	
		return -EINVAL;
	}	

	// reading completes after one run
	complete = true;

	printk(KERN_INFO "--- %s: (config) reading... \n", mod_name);

	char out_str[PROCFS_SIZE];
	int length = 0;
	length = snprintf(out_str, PROCFS_SIZE, "current clock format: %d%c", format_option, '\0');

	printk(KERN_INFO "--- %s: (config) trying to copy '%s'\n", mod_name, out_str);
	if (copy_to_user(buf, out_str, length))
	{
		printk(KERN_INFO "--- %s: (config_read) copy failed!\n", mod_name);
		return -1;	// which errno should be used here?
	}

	return length;
}

// handels writes to procfs_config.
// every input beyond the first char is disregarded.
// returns -1 on failure
//		-> buf may only point to '0' or '1'
static ssize_t config_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	if ('0' == *buf || '1' == *buf)
	{
		char tmp;
		if (copy_from_user(&tmp, buf, 1))
		{
			printk(KERN_INFO "--- %s: (config_write) copy failed!\n", mod_name);
			return -1;	// which errno should be used here?
		}
		format_option = tmp - '0';
	}
	else
	{
		printk(KERN_INFO "--- %s: format_option not set! only '0' or '1' allowed!\n", mod_name);
		return -1;
	}
	return 1;
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

static struct file_operations time_fops = {
	.owner =	THIS_MODULE,
	.read =		time_read,
//	.open =		time_open,
//	.release =	time_release,
};

static struct file_operations config_fops = {
	.owner =	THIS_MODULE,
	.read =		config_read,
	.write =	config_write,
//	.open =		time_open,
//	.release =	time_release,
};

// initialize module (executed when using insmod)
static int __init time_init(void)
{
	// create the procfs entries
	procfs_time = proc_create(
		"deeds_clock", 0444, 0, &time_fops);
	procfs_config = proc_create(
		"deeds_clock_config", 0666, 0, &config_fops);

	// check for null-pointers
	if (0 == procfs_time || 0 == procfs_config) 
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
	proc_remove(procfs_time);
	proc_remove(procfs_config);
	printk(KERN_INFO "--- %s is being unloaded.\n", mod_name);
}

module_init(time_init);
module_exit(time_cleanup);