#include "fifo.h"

struct fifo_dev;

/* read count bytes from the device
 * removes read bytes from the queue 
 *
 * @dev: the fifo device
 * @buf: the buffer to write to (user space)
 * @count: the number of bytes
 *
 * returns: 
 *	the number of bytes actualy read 
 *	-ENODEV if dev is a null pointer
 *	-EFAULT if copy from kernel to user space failed
 */
ssize_t fifo_read(struct fifo_dev* dev, char* buf, size_t count)
{
	// the bytes to read
	size_t read;
	size_t success_count;

	// will be copied to user space
	char* data_to_copy;
	// iterator for data_to_copy
	char* it;

	// local counter for dev->front
	size_t front = dev->front;

	if (0 == dev)
	{
		printk(KERN_INFO "--- fifo read failed: no device!\n");
		return -ENODEV;
	}

	if (dev->used)
	{
		printk(KERN_INFO "--- fifo write failed: fifo already in use!\n");
		return -EBUSY;
	}

	++dev->used;

	if (0 == dev->stored)
	{
		--dev->used;
		return 0;
	}

	if (count < dev->stored)
	{
		success_count = count;
		read = count;
	}
	else
	{
		success_count = dev->stored;
		read = dev->stored;
	}

	data_to_copy = kmalloc(success_count, GFP_KERNEL);
	it = data_to_copy;

	while (read--)
	{
		*it++ = *(dev->buffer + front);
		front = (front +1)%dev->size;
	}

	if (copy_to_user(buf, data_to_copy, success_count))
	{
		printk(KERN_INFO "--- fifo read failed: copy_to_user failed!\n");
		kfree(data_to_copy);
		--dev->used;
		return -EFAULT;
	}

	// update the device
	dev->front = front;
	dev->stored -= success_count;
	dev->read_bytes += success_count;

	kfree(data_to_copy);

	--dev->used;

	return success_count;
}

/* write count bytes to the device
 *
 * @dev: the fifo device
 * @buf: the buffer to read from (user space)
 * @count: the number of bytes
 *
 * returns: 
 * 	the number of bytes actualy written
 *	-ENODEV if dev is a null pointer
 *	-ENOBUFS if remaining buffer space is too small
 *	-EFAULT if copy from user to kernel space failed
 */
ssize_t fifo_write(struct fifo_dev* dev, const char* buf, size_t count)
{
	// copied data from user sapce
	char* user_data;
	// the user_data iterator
	char* it;

	size_t write = count;

	if (0 == dev)
	{
		printk(KERN_INFO "--- fifo write failed: no device!\n");
		return -ENODEV;
	}

	if (dev->used)
	{
		printk(KERN_INFO "--- fifo write failed: fifo already in use!\n");
		return -EBUSY;
	}

	++dev->used;

	if (count > dev->size - dev->stored)
	{
		printk(KERN_INFO "--- fifo write failed: not enough space remaining!\n");
		--dev->used;
		return -ENOBUFS;
	}

	user_data = kmalloc(count, GFP_KERNEL);
	it = user_data;
	if (copy_from_user(user_data, buf, count))
	{
		printk(KERN_INFO "--- fifo write failed: copy_from_user failed!\n");
		kfree(user_data);
		--dev->used;
		return -EFAULT;
	}

	while (write--)
	{
		*(dev->buffer + dev->end) = *it++;
		dev->end = (dev->end +1)%dev->size;
	}

	// update the device
	dev->stored += count;
	dev->write_bytes += count;

	kfree(user_data);

	--dev->used;

	return count;
}

/* resizes the buffer of the device 
 *
 * @dev: the fifo device
 * @new_size: new buffer size
 *
 * returns: 
 *	EINVAL if new_size is too small
 *	ENODEV if dev is a null pointer
 *	EINVAL if size > BUF_MAXSIZE
 *	0 on success
 */
int fifo_resize(struct fifo_dev* dev, size_t new_size)
{
	// the new buffer
	char* new_buf;
	// iterator
	char* it;
	// bytes to copy
	size_t left;

	if (0 == dev)
	{
		printk(KERN_INFO "--- fifo resize failed: no device!\n");
		return ENODEV;
	}

	if (dev->used)
	{
		printk(KERN_INFO "--- fifo resize failed: fifo already in use!\n");
		return -EBUSY;
	}

	++dev->used;

	if (new_size < dev->stored)
	{
		printk(KERN_INFO "--- fifo resize failed: new size too small!\n");
		--dev->used;
		return EINVAL;
	}

	if (new_size > BUF_MAXSIZE || new_size < BUF_MINSIZE)
	{
		printk(KERN_INFO "--- fifo resize failed: invalid size!\n");
		--dev->used;
		return EINVAL;
	}
	
	new_buf = kmalloc(new_size, GFP_KERNEL);
	it = new_buf;
	left = dev->stored;

	while (left--)
	{
		*it++ = *(dev->buffer + dev->front);
		dev->front = (dev->front +1)%dev->size; 
	}

	// set the right internals
	dev->front = 0;
	dev->end = dev->stored;
	dev->size = new_size;

	//cleanup and buffer change
	kfree(dev->buffer);
	dev->buffer = new_buf;

	--dev->used;

	return 0;
}

/* initializes the device, creates the buffer 
 *
 * @dev: the fifo device
 * @size: buffer size or 0 for default size (BUF_MAXSIZE)
 *
 * returns: 
 *	EPERM if device has allready been used 
 * 	ENODEV if dev is a null pointer
 *	EINVAL if size > BUF_MAXSIZE
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

	if (size > BUF_MAXSIZE || size < BUF_MINSIZE)
	{
		printk(KERN_INFO "--- fifo initialization failed: invalid size!\n");
		return EINVAL;
	}

	if (size < 1)
		dev->size = BUF_MAXSIZE;
	else
		dev->size = size;

	dev->stored = 0;
	dev->read_bytes = 0;
	dev->write_bytes = 0;

	dev->front = 0;
	dev->end = 0;

	dev->buffer = kmalloc(dev->size, GFP_KERNEL);

	dev->used = 0;

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

	kfree(dev->buffer);

	return 0;
}