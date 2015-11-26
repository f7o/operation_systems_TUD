#include "fifo.h"

struct fifo_dev;

/* read count bytes from the device
 * removes read bytes from the queue 
 *
 * @dev: the fifo device
 * @buf: the buffer to write to
 * @count: the number of bytes
 *
 * returns the number of bytes actualy read 
 */
ssize_t fifo_read(struct fifo_dev* dev, char* buf, size_t count)
{
	return 0;
}

/* write count bytes to the device
 *
 * @dev: the fifo device
 * @buf: the buffer to read from
 * @count: the number of bytes
 *
 * returns the number of bytes actualy written 
 */
ssize_t fifo_write(struct fifo_dev* dev, const char* buf, size_t count)
{
	return 0;
}

/* resizes the buffer of the device 
 *
 * @dev: the fifo device
 * @new_size: new buffer size
 *
 * returns -1 if new_size is too small or 0 on success
 */
ssize_t fifo_resize(struct fifo_dev* dev, size_t new_size)
{
	return 0;
}

/* initializes the device, creates the buffer 
 *
 * @dev: the fifo device
 * @size: buffer size
 *
 */
void fifo_init(struct fifo_dev* dev, size_t size)
{

}

/* destroys the device, frees the buffer
 *
 * @dev: the fifo device
 *
 */
void fifo_destroy(struct fifo_dev* dev)
{

}