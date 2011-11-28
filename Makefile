obj-m := ftpfs.o

PWD:= $(shell pwd)
KERNEL_SOURCES:= /usr/src/linux

ins: default 
	insmod ftpfs.ko
rm: 
	rmmod ftpfs

default:
	$(MAKE) -C $(KERNEL_SOURCES) M=$(PWD) modules

clean:
	rm -f *.o *.ko *.mod.c .*o.cmd

