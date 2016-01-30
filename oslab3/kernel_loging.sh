#!/bin/bash

while [[ true ]]; do
	dmesg >> ./kernel_log
	sudo dmesg -C
	sleep 2
done