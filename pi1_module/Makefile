obj-m := pi1.o

KERNELDIR := ~/linux-rpi/
ARM := ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(ARM) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(ARM) clean
copy:
	scp pi1.ko pi@10.10.10.13:~/
ssh:
	ssh pi@10.10.10.13
