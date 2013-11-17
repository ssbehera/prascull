obj-m = scull.o

KVERSION = `uname -r`

all:
	make -C /usr/src/kernels/$(KVERSION)/ M=`pwd` modules
clean:
	make -C /usr/src/kernels/$(KVERSION)/ M=`pwd` clean
