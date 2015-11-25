#ifndef INCLUDE_FIFO
#define INCLUDE_FIFO

ssize_t fifo_read(struct fifo_dev*, char*, size_t);
ssize_t fifo_wirte(struct fifo_dev*, const char*, size_t);

ssize_t fifo_resize(struct fifo_dev*);
void fifo_init(struct fifo_dev*, size_t);

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
};

#endif