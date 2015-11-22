obj-m += task11.o
obj-m += task12.o
PWD := $(shell pwd)
KVER := $(shell uname -r)

default:
	make -C /lib/modules/$(KVER)/build SUBDIRS=$(PWD) modules

clean:
	rm *.o *.ko *.mod.c *.symvers *.order
