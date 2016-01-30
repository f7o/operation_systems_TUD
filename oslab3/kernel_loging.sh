#!/bin/bash

sudo rm -f fifo_stats
sudo touch fifo_stats
while [[ true ]]; do
	dmesg >> ./kernel_log
	sudo dmesg -C
	printf "$(cat /proc/deeds_fifo_stats)\n" >> fifo_stats
	sleep 2
done
