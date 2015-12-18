#ifndef INCLUDE_FIFO
#define INCLUDE_FIFO

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>

#include <asm/uaccess.h>

#define BUF_STDSIZE 32

struct data_item {
	size_t qid;
	unsigned long long time;
	char * msg;
};

struct fifo_dev {

	// --- device info ---

	size_t size;
	size_t insertitions;
	size_t removals;
	unsigned long long seq_no;

	// --- internals ---

	// position of the first entry to read
	size_t front;
	// position of the first empty spot after the last element
	size_t end;

	// the device buffer
	struct data_item** buffer;

	// counting semaphores
	struct semaphore full;
	struct semaphore empty;

	// mutex protection for front and end
	struct mutex read;
	struct mutex write;
};

ssize_t fifo_read(struct fifo_dev* dev, struct data_item* item);
ssize_t fifo_write(struct fifo_dev* dev, struct data_item* item);

int fifo_init(struct fifo_dev*, size_t);
int fifo_destroy(struct fifo_dev*);

#endif