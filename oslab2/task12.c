#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Lab Solution Task 1.2");
MODULE_LICENSE("GPL");

int time_config;

static int get_deeds_seconds(struct seq_file *m, void *v)
{	
	struct timeval tv;
	do_gettimeofday(&tv);
	seq_printf(m, "current time: %ld seconds\n", tv.tv_sec);
	return 0;
}

static int get_deeds_pretty_seconds(struct seq_file *m, void *v)
{	
	struct timeval tv;
	struct tm pt;
	
	do_gettimeofday(&tv);
	time_to_tm(tv.tv_sec, 0, &pt);

	seq_printf(m, "current time: %ld-%02d-%02d  %02d:%02d:%02d\n", 
			pt.tm_year + 1900, pt.tm_mon + 1, pt.tm_mday, pt.tm_hour+ 1, pt.tm_min, pt.tm_sec);
	return 0;
}

// this method is called whenever the module is being used
// e.g. for both read and write operations
static int time_open(struct inode * inode, struct file * file)
{

	if(time_config == 0)
	{
		return single_open(file, get_deeds_seconds, NULL);
	}

	if(time_config == 1)
	{
		return single_open(file, get_deeds_pretty_seconds, NULL);
	}

	return -1;
}

static int get_time_config(struct seq_file *m, void *v)
{
	seq_printf(m, "current clock format: %d\n", time_config);
	return 0;
}

static int time_config_open(struct inode *inode, struct file *file)
{
	return single_open(file, get_time_config, NULL);
}

static ssize_t time_config_write(struct file *file, const char *buffer, size_t count, loff_t *pos)
{


	if(count > 1) 
	{	// only single character without any termination
		return -EINVAL;
	}

	printk(KERN_INFO "count: %lu", count); // look at the output, maybe somebody can explain it!
	if(copy_from_user(&time_config, buffer, 1))
	{
		return -EFAULT;
	}

	time_config -= 48; // UTF-8 encoding

	if(time_config == 1 || time_config == 0)
	{	
		return count; 
	}
	 else 
	{
		return -EINVAL;
	}

}

// module's file operations, a module may need more of these
static struct file_operations time_fops = {
	.owner =	THIS_MODULE,
	.read =		seq_read,
	.open =		time_open,
	.release =	single_release,
};

// modules file operations for the config file
static struct file_operations time_configuration_fops = {
	.owner = 	THIS_MODULE,
	.read =		seq_read,
	.write =	time_config_write,	
	.open = 	time_config_open,
	.release =	single_release,
};

// initialize module (executed when using insmod)
static int __init time_init(void)
{
	if(!proc_create("deeds_clock", 0, NULL, &time_fops)) 
	{ 
		return -ENOMEM;
	}
	
	if(!proc_create("deeds_clock_config", 0, NULL, &time_configuration_fops))
	{
		return -ENOMEM;
	}

	time_config = 0;
	
	printk(KERN_INFO "time module is being loaded.\n");
	return 0;
}

// cleanup module (executed when using rmmod)
static void __exit time_cleanup(void)
{
	remove_proc_entry("deeds_clock", NULL);
	remove_proc_entry("deeds_clock_config", NULL);
	
	printk(KERN_INFO "time module is being unloaded.\n");
}

module_init(time_init);
module_exit(time_cleanup);
