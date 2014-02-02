obj-m = scull.o

KVERSION = `uname -r`

all:
	make -C /usr/src/linux-headers-$(KVERSION)/ M=`pwd` modules
clean:
	make -C /usr/src/linux-headers-$(KVERSION)/ M=`pwd` clean
