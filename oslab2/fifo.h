#ifndef INCLUDE_FIFO
#define INCLUDE_FIFO

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

#define BUF_MAXSIZE 4096
#define BUF_MINSIZE 4

struct fifo_dev {

	// --- device info ---

	size_t size;
	size_t stored;
	size_t read_bytes;
	size_t write_bytes;

	// --- internals ---

	// position of the first char to be read
	size_t front;
	// position of the first empty spot after the last element
	size_t end;

	// the device buffer
	char* buffer;

	// used flag, as in: implementation of a race condition...
	int used;
};

ssize_t fifo_read(struct fifo_dev*, char*, size_t);
ssize_t fifo_write(struct fifo_dev*, const char*, size_t);

int fifo_resize(struct fifo_dev*, size_t);

int fifo_init(struct fifo_dev*, size_t);
int fifo_destroy(struct fifo_dev*);

#endif