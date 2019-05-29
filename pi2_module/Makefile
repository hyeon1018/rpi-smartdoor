obj-m := pi2.o

KDIR := ~/linux-rpi/
ARM := ARCH=arm CROSS_COMPILE=/usr/bin/arm-linux-gnueabi-
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) $(ARM) modules

copy:
	scp pi2.ko pi@10.10.10.13:~/

ssh :
	ssh pi@10.10.10.13

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) $(ARM) clean
