#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int size = 128;

void* _write(void* arg)
{
	int file;
	int success = 1;
	const char* s = "foo bar 42";

	while (1)
	{
		file = open("/dev/fifo0", O_WRONLY);
		if (-1 == file)
		{
			printf("write: open failed. error: %s\n", strerror(errno));
			usleep(490000);
			continue;
		}

		success = write(file, s, 10);
		if (success > 0)
			printf("write: success\n");
		else
			printf("write failed: error %s\n", strerror(-success));

		close(file);
		usleep(500000);
	}
	pthread_exit(0);
}

void* _read(void* arg)
{
	int file;
	int success = 1;
	char s[9];

	while (1)
	{
		file = open("/dev/fifo1", O_RDONLY);
		if (-1 == file)
		{
			printf("read: open failed. error: %s\n", strerror(errno));
			usleep(490000);
			continue;
		}

		success = read(file, s, 8);
		if (success > 0)
		{
			s[success] = '\0';
			printf("read: success; '%s'\n", s);
		}
		else if (0 == success)
			printf("read: nothing to read");
		else
			printf("read: error %s\n", strerror(-success));

		close(file);
		usleep(500000);
	}
	pthread_exit(0);
}

void* _resize(void* arg)
{
	int file;
	int success = 1;
	char s[4];

	while (1)
	{
		file = open("/proc/fifo_config", O_WRONLY);
		if (-1 == file)
		{
			printf("resize: open failed. error: %s\n", strerror(errno));
			usleep(490000);
			continue;
		}

		size = (size+1)%64 + 20;
		sprintf(s, "%d", size);

		success = write(file, s, 4);
		if (success > 0)
			printf("resize: tried size %s\n", s);

		close(file);
		usleep(500000);
	}
	pthread_exit(0);
}

int main()
{
	printf("creating threads...\n");
	pthread_t write;
	pthread_t read;
	pthread_t resize1;
	pthread_t resize2;

	int err = pthread_create(&write, 0, _write, 0);
	err = pthread_create(&read, 0, _read, 0);

	//usleep(150000);

	err = pthread_create(&resize1, 0, _resize, 0);
	err = pthread_create(&resize2, 0, _resize, 0);

	printf("threads created!\n");


	int ret;

	err = pthread_join(write, (void**)&ret);
	err = pthread_join(read, (void**)&ret);
	err = pthread_join(resize1, (void**)&ret);
	err = pthread_join(resize2, (void**)&ret);

	if (err)
		printf("thread join failed.\n");

	printf("main complete!\n");
	return 0;
}