obj-m := time_mod.o
ccflags-y := -Wall -std=gnu99 -Wno-declaration-after-statement
PWD := $(shell pwd)
KVER := $(shell uname -r)

default:
	make -C /lib/modules/$(KVER)/build SUBDIRS=$(PWD) modules

clean:
	make -C /lib/modules/$(KVER)/build SUBDIRS=$(PWD) modules clean
