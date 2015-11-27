obj-m := fifo_mod.o
#time_mod-y := time_mod.o
fifo_mod-y := fifo.o fifo_test.o

ccflags-y := -Wall -std=gnu99 -Wno-declaration-after-statement
PWD := $(shell pwd)
KVER := $(shell uname -r)

default:
	make -C /lib/modules/$(KVER)/build SUBDIRS=$(PWD) modules

clean:
	make -C /lib/modules/$(KVER)/build SUBDIRS=$(PWD) modules clean
