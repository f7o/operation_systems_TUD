#!/bin/bash
echo "--- please enter the desired interval_ms for the kernel consumer!"
echo "--- choose 167 for a full queue and around 50 for an empty queue."
read interval

_term(){
	echo "--- kill all"
	sudo ./kill_all.sh
	kill "$kernlog"
}

echo "--- delete user_log file from previous run"
sudo rm -f user_log
sudo touch user_log

echo "--- load lkms..."
sudo insmod fifo_lkm.ko size=43
sudo insmod producer_lkm1.ko interval_ms=500 msg="this_is_a_funny_message_hahaha" mod_name=kprod1
sudo insmod producer_lkm2.ko interval_ms=500 msg="this_is_a_funny_message_too..." mod_name=kprod2
sudo insmod consumer_lkm1.ko interval_ms="$interval" mod_name=kcons1
echo "--- all lkms loaded"

./kernel_loging.sh &
kernlog=$!

echo "--- execute all generic_user programs"
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
