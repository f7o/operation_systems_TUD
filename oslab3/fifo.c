#include "fifo.h"

struct data_item;
struct fifo_dev;

/**
 * Make a data_item from a string.
 * A call to this function allocates memory amd needs a complementing 
 * call to free_data_item at some point in time!
 * Side effect: str will not be copied and 
 *				therefore changes for the caller, too.
 * 
 * @str: the string specifying the data_item contents.
 *		 has to be of the right format: "0,[creation time],[a message]"
 *
 * returns:
 *	ERR_PTR(-EINVAL) if the creation time could not be parsed (from kstrtoull)
 * 	a pointer to the created struct
 */
struct data_item* alloc_di_str(char* str)
{
	int err;
	unsigned long long time;

	char* sub_str_begin = str;
	char* sub_str = str;

	// ignore everything until the first ','
	strsep(&sub_str, ","); 		// sub_str points right after the first ',' 
	if (0 == sub_str)
	{
		printk(KERN_INFO "--- malformed csv detected at first ','\n");
		return ERR_PTR(-EINVAL);
	}

	// get the creation time, let strsep create the needed zero termination
	sub_str_begin = sub_str;
	strsep(&sub_str, ",");		// sub_str points right after the second ','
	if (0 == sub_str)
	{
		printk(KERN_INFO "--- malformed csv detected at second ','\n");
		return ERR_PTR(-EINVAL);
	}

	err = kstrtoull(sub_str_begin, 0, &time);
	if (err)
	{
		printk(KERN_INFO "--- could not get time for data_item, string was: %s\n", sub_str_begin);
		return ERR_PTR(err);
	}

	return alloc_di(sub_str, time);
}

/**
 * Make a data_item from a message and the creation time.
 * A call to this function allocates memory amd needs a complementing 
 * call to free_di at some point in time!
 *
 * @msg: the string specifying the data_item msg.
 * @time: creation time of the struct or 0 to get the time during execution 
 *
 * returns:
 * 	a pointer to the created struct
 */
struct data_item* alloc_di(const char* msg, unsigned long long time)
{
	struct data_item* item;

	if (0 == msg)
	{
		printk(KERN_INFO "--- alloc_di: no null ptr msg possible!\n");
		return ERR_PTR(-EINVAL);
	}
	
	// allocate memory
	item = kmalloc(sizeof(struct data_item), GFP_KERNEL);

	item->qid = 0;

	// store the message
	item->msg = kmalloc(strlen(msg) * sizeof(char), GFP_KERNEL);
	strcpy(item->msg, msg);

	// store the time
	if (0 == time)
	{
		struct timeval tv;
		do_gettimeofday(&tv);
		item->time = tv.tv_sec;
	}
	else
		item->time = time;

	return item;
}
EXPORT_SYMBOL(alloc_di);

/**
 * Free the memory allocated to a data_item struct
 * 
 * @di: the data_item to deallocate
 */
void free_di(struct data_item* di)
{
	if (di != 0)
	{
		if (di->msg != 0)
			kfree(di->msg);
		else
			printk(KERN_INFO "free_di failed: null ptr msg\n");

		kfree(di);
	}
	else
		printk(KERN_INFO "free_di failed: null ptr data_item\n");
}
EXPORT_SYMBOL(free_di);

/** 
 * Read the first entry from the buffer.
 * This function may block, but times out at 5s waiting time!
 *
 * @dev: the fifo device
 *
 * returns: 
 *	ptr to the read data_item struct
 * 	ERR_PTR(-ENODEV) if dev is a null pointer
 * 	ERR_PTR(-ETIME) if there was nothing to read within 5 seconds of waiting
 *	ERR_PTR(-EINTR) if mutex lock was interrupted
 */
struct data_item* fifo_read(struct fifo_dev* dev)
{
	struct data_item* item;

	if (0 == dev)
	{
		printk(KERN_INFO "--- fifo_read failed: null ptr device!\n");
		return ERR_PTR(-ENODEV);
	}

	// block if empty
	if (down_timeout(&dev->empty, 5*HZ))
		return ERR_PTR(-ETIME);

	// block if another read is in progress
	if (mutex_lock_interruptible(&dev->read))
		return ERR_PTR(-EINTR);

	// read from the queue
	item = *(dev->buffer + dev->front);
	dev->front = (dev->front +1) % dev->size;
	up(&dev->full);

	// update stats
	++dev->removals;

	mutex_unlock(&dev->read);
	return item;
}

/**
 * Write a data_item to the buffer.
 * This function may block, but times out at 5s waiting time!
 *
 * @dev: the fifo device
 * @item: the data_item ptr which will be written
 *
 * returns: 
 *	0 on success
 *	-ENODEV if dev is a null pointer
 *	-ETIME if there was no empty slot within 5 seconds of waiting
 *	-EINTR if mutex/semaphore locking was interrupted
 */
int fifo_write(struct fifo_dev* dev, struct data_item* item)
{
	if (0 == dev)
	{
		printk(KERN_INFO "--- fifo_write failed: null ptr device!\n");
		return -ENODEV;
	}

	// block if queue is full
	if (down_timeout(&dev->full, 5*HZ))
		return -ETIME;

	// block if another write is in progress
	if (mutex_lock_interruptible(&dev->write))
		return -EINTR;

	// prepare the data_item struct
	item->qid = dev->seq_no;

	// write to the queue
	*(dev->buffer + dev->end) = item;
	dev->end = (dev->end +1) % dev->size;
	up(&dev->empty);

	// update stats
	++dev->insertitions;
	++dev->seq_no;

	mutex_unlock(&dev->write);
	return 0;
}

/**
 * initializes the device, creates the buffer 
 *
 * @dev: the fifo device
 * @size: buffer size or 0 for default size (BUF_STDSIZE)
 *
 * returns: 
 *	-EPERM if device has already been used 
 * 	-ENODEV if dev is a null pointer
 * 	0 on success
 */
int fifo_init(struct fifo_dev* dev, size_t size)
{
	if (0 == dev)
	{
		printk(KERN_INFO "--- fifo initialization failed: no device!\n");
		return -ENODEV;
	}

	if (dev->buffer != 0)
	{
		printk(KERN_INFO "--- fifo reinitialization not permitted!\n");
		return -EPERM;
	}

	if (size < 1)
		dev->size = BUF_STDSIZE;
	else
		dev->size = size;

	/*
	 * init semas:
	 * fifo_write needs to aquire dev.full to be able to write a
	 * data_item to the queue. (-> init with dev.size)
	 * after writing to the queue, an up() call is performed on empty
	 *
	 * fifo_read needs to aquire dev.empty to be able to read a
	 * data_item from the queue. (-> init with 0)
	 * after reading from the queue, an up() call is performed on full
	 */
	sema_init(&dev->full, dev->size);
	sema_init(&dev->empty, 0);

	/*
	 * only needed to ensure that there is one and only one access at
	 * each end of the fifo. (-> prevents data corruption)
	 */ 
	mutex_init(&dev->read);
	mutex_init(&dev->write);

	dev->insertitions = 0;
	dev->removals = 0;
	dev->seq_no = 0;

	dev->front = 0;
	dev->end = 0;

	dev->buffer = kmalloc(dev->size * sizeof(struct data_item*), GFP_KERNEL);

	return 0;
}

/**
 * destroys the device, frees the buffer
 *
 * @dev: the fifo device
 *
 * returns:
 * 	-ENODEV if dev is a null pointer
 *	0 on success
 */
int fifo_destroy(struct fifo_dev* dev)
{
	if (0 == dev)
	{
		printk(KERN_INFO "--- fifo destruction failed: no device!\n");
		return -ENODEV;
	}

	// forbid all read or write attempts
	mutex_lock(&dev->write);
	mutex_lock(&dev->read);

	// free the remaining data_item structs
	while (dev->front != dev->end)
	{
		kfree(*(dev->buffer + dev->front));
		dev->front = (dev->front +1) % dev->size;
	}

	// kill all mutexes
	mutex_unlock(&dev->read);
	mutex_destroy(&dev->read);
	mutex_unlock(&dev->write);
	mutex_destroy(&dev->write);

	// do the same for all semas?

	kfree(dev->buffer);

	return 0;
}