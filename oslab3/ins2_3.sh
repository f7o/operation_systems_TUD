#!/bin/bash

_term(){
	echo "=== kill all user processes"
	kill "$uprod1"
	kill "$uprod2"
	kill "$uprod3"
	kill "$ucons1"
	kill "$ucons2"
	kill "$kernlog"
}

echo "=== delete files from previous run"

rm ./user_log
rm ./kernel_log
sudo dmesg -C

echo "=== load lkms"

sudo insmod fifo_lkm.ko size=43
sudo insmod producer_lkm1.ko interval_ms=500 msg="this_is_a_funny_message_hahaha" mod_name=kprod1
sudo insmod producer_lkm2.ko interval_ms=500 msg="this_is_a_funny_message_too..." mod_name=kprod2
sudo insmod consumer_lkm1.ko interval_ms=167 mod_name=kcons1

echo "=== all lkms loaded, executing generic_users..."

./kernel_loging.sh &
kernlog=$!
./generic_user -p "user1_is_funny" -i 333 -n "uprod1" &
uprod1=$!
./generic_user -p "user2_is_very_funny" -i 333 -n "uprod2" &
uprod2=$!
./generic_user -p "user3_is_NOT_funny!" -i 200 -n "uprod3" &
uprod3=$!
./generic_user -i 500 -n "ucons1" >> ./user_log &
ucons1=$!
./generic_user -i 250 -n "ucons2" >> ./user_log &
ucons2=$!

trap _term SIGINT

wait $kernlog
