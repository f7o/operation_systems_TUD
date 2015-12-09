#!/bin/bash

while true
  do
    cat /proc/deeds_clock & echo -n "0" > /proc/deeds_clock_config &
    cat /proc/deeds_clock_config
    cat /proc/deeds_clock & echo -n "1" > /proc/deeds_clock_config &
    cat /proc/deeds_clock_config
  done
