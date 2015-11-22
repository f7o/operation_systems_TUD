#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Lab Solution");
MODULE_LICENSE("GPL");

static int get_deeds_seconds(struct seq_file *m, void *v)
{	
	struct timeval tv;
	do_gettimeofday(&tv);
	seq_printf(m, "current time: %ld seconds\n", tv.tv_sec);
	return 0;
}

// this method is called whenever the module is being used
// e.g. for both read and write operations
static int time_open(struct inode * inode, struct file * file)
{
	return single_open(file, get_deeds_seconds, NULL);
}

// module's file operations, a module may need more of these
static struct file_operations time_fops = {
	.owner =	THIS_MODULE,
	.read =		seq_read,
	//.write =	time_write,
	.open =		time_open,
	.release =	single_release,
};

// initialize module (executed when using insmod)
static int __init time_init(void)
{
	if(!proc_create("deeds_clock", 0, NULL, &time_fops)) 
	{ 
		return -ENOMEM;
	}
	
	printk(KERN_INFO "time module is being loaded.\n");
	return 0;
}

// cleanup module (executed when using rmmod)
static void __exit time_cleanup(void)
{
	remove_proc_entry("deeds_clock", NULL);
	printk(KERN_INFO "time module is being unloaded.\n");
}

module_init(time_init);
module_exit(time_cleanup);
