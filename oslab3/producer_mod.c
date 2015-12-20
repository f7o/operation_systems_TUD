#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>		// time
#include <linux/errno.h>		
#include <linux/fs.h>			// file ops struct
#include <linux/proc_fs.h>		// proc_dir_entry
#include <linux/seq_file.h>		// used for procfs read
#include <linux/workqueue.h>
#include <linux/moduleparam.h>

struct data_item;

MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Lab Solution");
MODULE_LICENSE("GPL");

// -------- globals ------------------------------------------------------
// proc pointer
static struct proc_dir_entry* proc_exec;

// work queue struct ptr
static struct workqueue_struct* wqs;

// data_item
static struct data_item* di;

// msg of the LKM
static char* msg = "this is a msg";
module_param(msg, charp, 0);

// name of the LKM
static char* mod_name = "kernel_producer";
module_param(mod_name, charp, 0);

// module parameter to configure the fifo size
static int rate = 2;
module_param(rate, int, 0);
// -------- globals end --------------------------------------------------

// -------- force execution ----------------------------------------------

// import from other modules
extern int put(struct data_item*);
extern struct data_item* alloc_di(char*, unsigned long long);

void produce(struct work_struct* ws)
{
	int err = 0;
	struct timeval tv;
	do_gettimeofday(&tv);

	di = alloc_di(msg, tv.tv_sec);

	err = put(di);
	if (err)
		printk(KERN_INFO "--- %s: put failed!\n", mod_name);
}

static int exec_read(struct seq_file* seq, void* v)
{
	DECLARE_DELAYED_WORK(work, produce);

	seq_printf(seq, "%s: starting production!", mod_name);
	
	while (1)
	{
		queue_delayed_work(wqs, &work, rate*100*HZ);
	}

	return 0;
}

static int exec_open(struct inode* inode, struct file* filp)
{
	return single_open(filp, exec_read, 0);
}

/* 
 * The file ops for user space stats of the queue
 */
static struct file_operations exec_fops = {
	.owner =	THIS_MODULE,
	.open =		exec_open,
	.read =		seq_read,
	.release =	single_release,
};
// -------- force execution end ------------------------------------------

/*
 * initializes the LKM
 * calls functions to create/init the following:
 *		the exec file in /proc
 *		the work queue
 */
static int __init producer_mod_init(void)
{
	wqs = alloc_workqueue(mod_name, WQ_UNBOUND, 1);
	if (0 == wqs)
	{
		printk(KERN_INFO "--- %s: work queue creation failed!\n", mod_name);	
		proc_remove(proc_exec);
		return -ENOMEM;
	}
	
	proc_exec = proc_create(
		mod_name, 0444, 0, &exec_fops);

	if (0 == proc_exec) 
	{
		printk(KERN_INFO "--- %s: creation of /proc/%s failed!\n", mod_name, mod_name);
		return -ENOMEM;
	}

	printk(KERN_INFO "--- %s: is being loaded.\n", mod_name);
	return 0;
}

static void __exit producer_mod_cleanup(void)
{
	proc_remove(proc_exec);
	flush_workqueue(wqs);
	destroy_workqueue(wqs);

	printk(KERN_INFO "--- %s: is being unloaded.\n", mod_name);
}

module_init(producer_mod_init);
module_exit(producer_mod_cleanup);