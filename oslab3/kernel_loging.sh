#!/bin/bash

echo "--- preping kernel logs for this run..."
sudo rm -f fifo_stats
sudo touch fifo_stats
sudo rm -f kernel_log
sudo touch kernel_log
sudo dmesg -C
echo "--- loging starts!"

while [[ true ]]; do
	dmesg >> ./kernel_log
	sudo dmesg -C
	printf "$(cat /proc/deeds_fifo_stats)\n" >> fifo_stats
	sleep 2
done
