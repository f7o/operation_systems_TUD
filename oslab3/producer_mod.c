#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>		// time
#include <linux/module.h>
#include <linux/errno.h>		
#include <linux/workqueue.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>

// forward declarations
struct data_item;
void produce(struct work_struct*);

MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Lab Solution");
MODULE_LICENSE("GPL");

// -------- globals ------------------------------------------------------
// work queue struct ptr
static struct workqueue_struct* wqs;

// msg of the LKM
static char* msg = "kernel_msg_foobar";
module_param(msg, charp, 0);

// name of the LKM
static char* mod_name = "kprod";
module_param(mod_name, charp, 0);

// module parameter to configure the fifo size
static int interval_ms = 1000;
module_param(interval_ms, int, 0);

// termination controll
static int continue_exec = 1;

// delayed work item
DECLARE_DELAYED_WORK(work, produce);
// -------- globals end --------------------------------------------------

// import from other modules
extern int put(struct data_item*, const char*);
extern void free_di(struct data_item*);
extern struct data_item* alloc_di(const char*, unsigned long long);

void produce(struct work_struct* ws)
{
	int err = 0;
	struct data_item* di;
	struct timeval tv;

	do_gettimeofday(&tv);

	di = alloc_di(msg, tv.tv_sec);

	err = put(di, THIS_MODULE->name);
	if (err)
	{
		free_di(di);
		if (-ETIME == PTR_ERR(di))
			printk(KERN_INFO "--- %s: put timeout.\n", mod_name);
		else
			printk(KERN_INFO "--- %s: put failed.\n", mod_name);
	}

	if (continue_exec)
		queue_delayed_work(wqs, &work, interval_ms*HZ/1000);
}

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
		return -ENOMEM;
	}

	queue_delayed_work(wqs, &work, interval_ms*HZ/1000);

	printk(KERN_INFO "--- %s: loaded successfully.\n", mod_name);
	return 0;
}

static void __exit producer_mod_cleanup(void)
{
	continue_exec = 0;

	printk(KERN_INFO "--- %s: unloading...\n", mod_name);
	printk(KERN_INFO "--- %s: waiting for work to finish...\n", mod_name);
	cancel_delayed_work_sync(&work);

	destroy_workqueue(wqs);
	printk(KERN_INFO "--- %s: unloading complete!\n", mod_name);
}

module_init(producer_mod_init);
module_exit(producer_mod_cleanup);