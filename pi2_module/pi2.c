#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/kmod.h>

#define PIN1 4
#define PIN2 17
#define PIN3 27
#define PIN4 22
#define STEPS 8
/*
#define DEV_NAME "pi2_dev"
#define IOCTL_START_NUM 0x80
#define IOCTL_MOTOR IOCTL_START_NUM+1
#define IOCTL_FORCE IOCTL_START_NUM+2
#define IOCTL_NUM 'z'
#define MOTOR _IOWR(SIMPLE_IOCTL_NUM, IOCTL_MOTOR, unsigned long *)
#define FORCE _IOWR(SIMPLE_IOCTL_NUM, IOCTL_FORCE, unsigned long *)
*/

MODULE_LICENSE("GPL");

int steps[STEPS][4] = {
	{1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0},
	{0, 0, 1, 0}, {0, 0, 1, 1}, {0, 0, 0, 1}, {1, 0, 0, 1}
};

static int door_state = 0; //0:closed 1: opened
static struct timer_list motor_timer;
//static dev_t dev_num;
//static struct cdev *cd_cdev;

static void setStep(int pin1, int pin2, int pin3, int pin4){
	gpio_set_value(PIN1, pin1);
	gpio_set_value(PIN2, pin2);
	gpio_set_value(PIN3, pin3);
	gpio_set_value(PIN4, pin4);
}

static void door_open(void){
	int i = 0, j = 0;

	if(door_state == 0){
		setStep(0, 0, 0, 0);

		for(i = 0; i < STEPS * 64; i++){
			for(j = 0; j < 8; j++){
				setStep(steps[j][0], steps[j][1], steps[j][2], steps[j][3]);
				mdelay(1);
			}
		}

		door_state = 1;
	}
}

static void door_close(unsigned long data){
	int i = 0, j = 0;

	if(door_state == 1){
		setStep(0, 0, 0, 0);

		for(i = 0; i < STEPS * 64; i++){
			for(j = 7; j >= 0; j--){
				setStep(steps[j][0], steps[j][1], steps[j][2], steps[j][3]);
				mdelay(1);
			}
		}

		door_state = 0;
	}
}

static void motor(int pir){
	if(pir){
		if(timer_pending(&motor_timer)){
			del_timer(&motor_timer);
		}
		door_open();
	}	//open: 1
	else{
		if(!timer_pending(&motor_timer)){
			motor_timer.function = door_close;
			motor_timer.data = 0L;
			motor_timer.expires = jiffies + (10 * HZ); //can change
			add_timer(&motor_timer);
		}
	}	//close: 0
}
//pir data handler

static void force(int oc){
	if(timer_pending(&motor_timer)){
		del_timer(&motor_timer);
	}

	if(oc){
		door_open();

		motor_timer.function = door_close;
		motor_timer.data = 0L;
		motor_timer.expires = jiffies + (5 * HZ); //can change
		add_timer(&motor_timer);
	}
	else{
		door_close(0);
	}
}
//oc = open close
/*
static long door_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
	int pir = 0;
	int oc = 0;

	switch(cmd){
		case MOTOR:
			pir = (int)arg;
			motor(pir);
			break;
		case FORCE:
			oc = (int)arg;
			force(oc);
			break;
		default:
			return -1;
	}

	return 0;
}

struct file_operations simple_char_fops = {
	.unlocked_ioctl = door_ioctl,
};
*/

static int __init pi2_init(void){
/*
	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &simple_char_fops);
	cdev_add(cd_cdev, dev_num, 1);
*/
	gpio_request_one(PIN1, GPIOF_OUT_INIT_LOW, "p1");
	gpio_request_one(PIN2, GPIOF_OUT_INIT_LOW, "p2");
	gpio_request_one(PIN3, GPIOF_OUT_INIT_LOW, "p3");
	gpio_request_one(PIN4, GPIOF_OUT_INIT_LOW, "p4");

	init_timer(&motor_timer);
/*
	motor(1);
	force(0);
	force(1);
	motor(0);
*/
//test run
	return 0;
}

static void __exit pi2_exit(void){
//	cdev_del(cd_cdev);
//	unregister_chrdev_region(dev_num, 1);

	gpio_free(PIN1);
	gpio_free(PIN2);
	gpio_free(PIN3);
	gpio_free(PIN4);

	del_timer(&motor_timer);
}

module_init(pi2_init);
module_exit(pi2_exit);
