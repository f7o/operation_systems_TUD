obj-m := fifo_lkm.o
fifo_lkm-y := fifo.o fifo_mod.o

ccflags-y := -Wall
PWD := $(shell pwd)
KVER := $(shell uname -r)

default:
	make -C /lib/modules/$(KVER)/build SUBDIRS=$(PWD) modules

clean:
	make -C /lib/modules/$(KVER)/build SUBDIRS=$(PWD) modules clean
