#include "fifo.h"

struct data_item;
struct fifo_dev;

/* read the first entry from the buffer
 *
 * @dev: the fifo device
 * @item: output for the data_item which was read
 *
 * returns: 
 *	0 on success
 *	-ENODEV if dev is a null pointer
 */
ssize_t fifo_read(struct fifo_dev* dev, struct data_item* item)
{
	return 0;
}

/* read the first entry from the buffer
 *
 * @dev: the fifo device
 * @item: the data_item which will be written
 *
 * returns: 
 *	0 on success
 *	-ENODEV if dev is a null pointer
 */
ssize_t fifo_write(struct fifo_dev* dev, struct data_item* item)
{
	return 0;
}

/* initializes the device, creates the buffer 
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

/* destroys the device, frees the buffer
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

	// free the remaining data_items!

	kfree(dev->buffer);

	return 0;
}