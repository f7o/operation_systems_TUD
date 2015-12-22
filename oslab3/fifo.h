#ifndef INCLUDE_FIFO
#define INCLUDE_FIFO

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/string.h>

#include <asm/uaccess.h>

#include "data_item.h"

#define BUF_STDSIZE 32

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

	// unblock parameters
	int kill;
	const char* mod_to_kill;

	// the device buffer
	struct data_item** buffer;

	// counting semaphores
	struct semaphore full;
	struct semaphore empty;

	// semaphore used during kill requests
	struct semaphore wait_on_kill;

	/* 
	 * mutex protection for front (removals, kill_read) 
	 * and end (insertitions, seq_no, kill_write)
	 */
	struct mutex read;
	struct mutex write;
};

struct data_item* alloc_di(const char*, unsigned long long);
struct data_item* alloc_di_str(char* str);
void free_di(struct data_item*);

struct data_item* fifo_read(struct fifo_dev*, const char*);
int fifo_write(struct fifo_dev*, struct data_item*, const char*);

int fifo_request_kill_read(struct fifo_dev*, const char*);
int fifo_request_kill_write(struct fifo_dev*, const char*);

int fifo_init(struct fifo_dev*, size_t);
int fifo_destroy(struct fifo_dev*);

#endif