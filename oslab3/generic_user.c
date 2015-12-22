#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

int interval_ms = 1000;
char* name = "gneric_user";
char* msg = "this is a message";
size_t size_receive = 256;
int producer = 0;

void produce(void)
{
	int file;
	int success = 0;
	size_t size = 42 + strlen(msg);
	char* csv = malloc(size * sizeof(char));

	struct timeval tv;

	while (1)
	{
		if (interval_ms < 1000)
			usleep(interval_ms*1000);
		else
			sleep(interval_ms/1000);

		file = open("/dev/deeds_fifo", O_WRONLY);
		if (-1 == file)
		{
			fprintf(stderr, "%s: open failed. error: %s\n", name, strerror(errno));
			continue;
		}

		gettimeofday(&tv, 0);
		snprintf(csv, size, "0,%ld,%s", tv.tv_sec, msg);

		success = write(file, csv, size);
		if (success < 0)
			fprintf(stderr, "%s: write failed. error: %s\n", name, strerror(errno));

		close(file);
	}

	free(csv);
}

void consume(void)
{
	int file;
	int success = 0;
	char* csv = malloc(size_receive * sizeof(char));

	while (1)
	{
		if (interval_ms < 1000)
			usleep(interval_ms*1000);
		else
			sleep(interval_ms/1000);

		file = open("/dev/deeds_fifo", O_RDONLY);
		if (-1 == file)
		{
			fprintf(stderr, "%s: open failed. error: %s\n", name, strerror(errno));
			continue;
		}

		success = read(file, csv, size_receive);
		if (success < 0)
		{
			fprintf(stderr, "%s: read failed. error: %s\n", name, strerror(errno));
			continue;
		}

		printf("[%s]%s\n", name, csv);

		close(file);
	}

	free(csv);
}

int main(int argc, char* const* argv)
{
	char c;

	while ((c = getopt(argc, argv, "p:r:n:s:")) != -1)
	{
		printf("%c\n", c);
		switch (c)
		{
		case 'p':
			producer = 1;
			msg = optarg;
			break;
		case 'r':
			interval_ms = atoi(optarg);
			break;
		case 'n':
			name = optarg;
			break;
		case 's':
			size_receive = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Usage: [-p msg] [-r interval_ms] [-n name] [-s size_receive]\n");
			return -1;
		}
	}

	if (optind > argc)
	{
		fprintf(stderr, "Some option arguments are missing!\n");
		return -1;
	}

	if (producer)
		produce();
	else
		consume();
	return 0;
}