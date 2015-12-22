obj-m := fifo_lkm.o producer_lkm1.o producer_lkm2.o consumer_lkm1.o
fifo_lkm-y := fifo.o fifo_mod.o
producer_lkm1-y := producer_mod.o
producer_lkm2-y := producer_mod.o
consumer_lkm1-y := consumer_mod.o 

ccflags-y := -Wall
PWD := $(shell pwd)
KVER := $(shell uname -r)

default:
	make -C /lib/modules/$(KVER)/build SUBDIRS=$(PWD) modules

clean:
	make -C /lib/modules/$(KVER)/build SUBDIRS=$(PWD) modules clean
