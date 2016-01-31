#!/bin/bash

sudo kill -9 $(pgrep generic | tr "\n" " ")
sudo rmmod producer_lkm1 
sudo rmmod producer_lkm2
sudo rmmod consumer_lkm1
sudo rmmod fifo_lkm
