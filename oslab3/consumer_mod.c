#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>		
#include <linux/workqueue.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include "data_item.h"

// forward declarations
void consume(struct work_struct*);

MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Lab Solution");
MODULE_LICENSE("GPL");

// -------- globals ------------------------------------------------------
// work queue struct ptr
static struct workqueue_struct* wqs;

// name of the LKM
static char* mod_name = "kcons";
module_param(mod_name, charp, 0);

// module parameter to configure the fifo size
static int interval_ms = 1000;
module_param(interval_ms, int, 0);

// termination controll
static int continue_exec = 1;

// delayed work item
DECLARE_DELAYED_WORK(work, consume);
// -------- globals end --------------------------------------------------

// import from other modules
extern struct data_item* get(void);
extern void free_di(struct data_item*);

void consume(struct work_struct* ws)
{
	struct data_item* di = get();
	if (IS_ERR(di))
	{
		if (-ETIME == PTR_ERR(di))
			printk(KERN_INFO "--- %s: get timeout.\n", mod_name);
		else
			printk(KERN_INFO "--- %s: get failed.\n", mod_name);
	}
	else
	{
		printk(KERN_INFO "[%s][%lu][%llu] %s\n",
					mod_name, di->qid, di->time, di->msg);
		free_di(di);	
	}

	if (continue_exec)
		queue_delayed_work(wqs, &work, interval_ms*HZ/1000);
}

/*
 * initializes the LKM
 * calls functions to create/init the following:
 *		the work queue
 */
static int __init consumer_mod_init(void)
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

static void __exit consumer_mod_cleanup(void)
{
	continue_exec = 0;

	printk(KERN_INFO "--- %s: unloading...\n", mod_name);
	printk(KERN_INFO "--- %s: waiting for work to finish...\n", mod_name);
	cancel_delayed_work_sync(&work);

	destroy_workqueue(wqs);
	printk(KERN_INFO "--- %s: unloading complete!\n", mod_name);
}

module_init(consumer_mod_init);
module_exit(consumer_mod_cleanup);