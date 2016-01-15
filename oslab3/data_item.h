#ifndef INCLUDE_DATA_ITEM
#define INCLUDE_DATA_ITEM

/*
 * inside an extra header to include only this struct in consumer_mod.c
 */
struct data_item {
	size_t qid;
	unsigned long long time;
	char* msg;
};

#endif