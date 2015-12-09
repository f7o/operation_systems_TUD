#!/bin/bash

while true
  do
    echo -n "0" > /proc/deeds_clock_config 
    echo -n "1" > /proc/deeds_clock_config 
  done
