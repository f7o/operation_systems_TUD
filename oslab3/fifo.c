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
 *	ERR_PTR() if the creation time could not be parsed (from kstrtoull)
 * 	a pointer to the created struct
 */
struct data_item* alloc_di_str(char* str)
{
	int err;
	unsigned long long time;

	char* msg;
	char* sub_str_begin = str;
	char* sub_str = str;

	// ignore everything until the first ','
	strsep(&sub_str, ","); 		// sub_str points right after the first ',' 
	if (0 == sub_str)
		return ERR_PTR(-EINVAL);

	// get the creation time, let strsep create the needed zero termination
	sub_str_begin = sub_str;
	strsep(&sub_str, ",");		// sub_str points right after the second ','
	if (0 == sub_str)
		return ERR_PTR(-EINVAL);

	err = kstrtoull(sub_str_begin, 0, &time);
	if (err)
	{
		printk(KERN_INFO "--- could not get time for data_item, string was: %s\n", sub_str_begin);
		return ERR_PTR(-err);
	}

	msg = kmalloc(strlen(sub_str) * sizeof(char), GFP_KERNEL);
	strcpy(msg, sub_str);

	return alloc_di(msg, time);
}

/**
 * Make a data_item from a message and the creation time.
 * A call to this function allocates memory amd needs a complementing 
 * call to free_di at some point in time!
 *
 * @str: the string specifying the data_item msg.
 * @time: creation time of the struct or 0 to get the time during execution 
 *
 * returns:
 * 	a pointer to the created struct
 */
struct data_item* alloc_di(char* msg, unsigned long long time)
{
	// allocate memory
	struct data_item* item = kmalloc(sizeof(struct data_item), GFP_KERNEL);

	item->qid = 0;
	item->msg = msg;

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

/**
 * Free the memory allocated to a data_item struct
 * 
 * @di: the data_item to deallocate
 */
void free_di(struct data_item* di)
{
	kfree(di->msg);
	kfree(di);
}

/** 
 * Read the first entry from the buffer.
 * This function may block!
 *
 * @dev: the fifo device
 *
 * returns: 
 *	ptr to the read struct
 *	ERR_PTR(ENODEV) if dev is a null pointer
 *	ERR_PTR(EINTR) if mutex/semaphore locking was interrupted
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
	if (down_interruptible(&dev->empty))
		return ERR_PTR(-EINTR);

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
 * read the first entry from the buffer
 *
 * @dev: the fifo device
 * @item: the data_item ptr which will be written
 *
 * returns: 
 *	0 on success
 *	ENODEV if dev is a null pointer
 *	EINTR if mutex/semaphore locking was interrupted
 */
int fifo_write(struct fifo_dev* dev, struct data_item* item)
{
	if (0 == dev)
	{
		printk(KERN_INFO "--- fifo_write failed: null ptr device!\n");
		return ENODEV;
	}

	// block if queue is full
	if (down_interruptible(&dev->full))
		return EINTR;

	// block if another write is in progress
	if (mutex_lock_interruptible(&dev->write))
		return EINTR;

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
 *	EPERM if device has allready been used 
 * 	ENODEV if dev is a null pointer
 * 	0 on success
 */
int fifo_init(struct fifo_dev* dev, size_t size)
{
	if (0 == dev)
	{
		printk(KERN_INFO "--- fifo initialization failed: no device!\n");
		return ENODEV;
	}

	if (dev->buffer != 0)
	{
		printk(KERN_INFO "--- fifo reinitialization not permitted!\n");
		return EPERM;
	}

	if (size < 1)
		dev->size = BUF_STDSIZE;
	else
		dev->size = size;

	sema_init(&dev->full, dev->size);
	sema_init(&dev->empty, 0);

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
 * 	ENODEV if dev is a null pointer
 *	0 on success
 */
int fifo_destroy(struct fifo_dev* dev)
{
	if (0 == dev)
	{
		printk(KERN_INFO "--- fifo destruction failed: no device!\n");
		return ENODEV;
	}

	// forbid all read or write attempts
	mutex_lock(&dev->write);
	mutex_lock(&dev->read);

	// free the remaining data_item structs
	while (dev->front != dev->end)
	{
		kfree(*(dev->buffer + dev->front));
		++dev->front;
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