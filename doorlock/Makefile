obj-m := doorlock_dev.o

KDIR := ~/linux-rpi/
ARM := ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) $(ARM) modules

copy:
	scp doorlock_dev.ko mknod.sh pi@10.10.10.13:~/

ssh :
	ssh pi@10.10.10.13

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) $(ARM) clean
