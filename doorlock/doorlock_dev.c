#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>

//keymatrix
#define K_SCAN1 6
#define K_SCAN2 13
#define K_SCAN3 19
#define K_SCAN4 26
#define K_IN1 12
#define K_IN2 16
#define K_IN3 20
#define K_IN4 21

#define PW_MAX_LENGTH 128

//motor
#define PIN1 4
#define PIN2 17
#define PIN3 27
#define PIN4 22
#define STEPS 8

//etc.
#define SPEAKER 5
#define LED 14
#define PIR 15
#define LIGHT_CLK 18
#define LIGHT_IN 23
#define LIGHT_OUT 24
#define LIGHT_EN 25

#define SPEAKER_HZ 700
#define LIGHT_MIN 500           // min value of light sensor for LED on
#define TIMER_SEC 10            // time of light lasts on

#define SPI_CLK_LENGTH 24
#define SPI_REQ_DATA 0x60
#define SPI_CLK 10

#define DEV_NAME "doorlock_dev"

MODULE_LICENSE("GPL");

///////////////////////// declare key matrix /////////////////////////
spinlock_t event_lock, timer_lock;
wait_queue_head_t wait_queue;
struct task_struct * keypad_task = NULL;

char * password = NULL;
char * newpass = NULL;
int msg = 0;
int pos = 0;
int mode = 0; // 1 when change password.

///////////////////////// declare motor /////////////////////////
int steps[STEPS][4] = {
	{1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0},
	{0, 0, 1, 0}, {0, 0, 1, 1}, {0, 0, 0, 1}, {1, 0, 0, 1}
};

static struct timer_list motor_timer;

static void motor_action(int oc);
static void keep_door_open(void);
static void alert_open(unsigned long data);

///////////////////////// declare etc /////////////////////////

static int irq_pir;
//light is 1 when light has to turn on
static int door_state, light_state = 0;
static struct timer_list human_timer;

static void timer_reset(void);

///////////////////////// gpios /////////////////////////
static struct gpio dev_gpios[19] = {{LED, GPIOF_OUT_INIT_LOW, "led"},
				{PIR, GPIOF_IN, "pir"},
				{LIGHT_CLK, GPIOF_INIT_LOW, "light_clk"},
				{LIGHT_IN, GPIOF_IN, "light_in"},
				{LIGHT_OUT, GPIOF_INIT_LOW, "light_data_out"},
				{LIGHT_EN, GPIOF_INIT_HIGH, "light_enable"},
				{SPEAKER, GPIOF_OUT_INIT_LOW, "speaker"},
				{K_SCAN1, GPIOF_OUT_INIT_LOW, "key_scan_1"},
				{K_SCAN2, GPIOF_OUT_INIT_LOW, "key_scan_2"},
				{K_SCAN3, GPIOF_OUT_INIT_LOW, "key_scan_3"},
				{K_SCAN4, GPIOF_OUT_INIT_LOW, "key_scan_4"},
				{K_IN1, GPIOF_IN, "key_in_1"},
				{K_IN2, GPIOF_IN, "key_in_2"},
				{K_IN3, GPIOF_IN, "key_in_3"},
				{K_IN4, GPIOF_IN, "key_in_4"},
				{PIN1, GPIOF_OUT_INIT_LOW, "p1"},
				{PIN2, GPIOF_OUT_INIT_LOW, "p2"},
				{PIN3, GPIOF_OUT_INIT_LOW, "p3"},
				{PIN4, GPIOF_OUT_INIT_LOW, "p4"}};

static void beep(int length, int count) {
	int i, j;
	for (i = 0 ; i < count ; i++){
		for (j = 0 ; j < 100 * length ; j++){
			gpio_set_value(SPEAKER, 1);
			udelay(SPEAKER_HZ);
			gpio_set_value(SPEAKER, 0);
			udelay(SPEAKER_HZ);
		}
		if(i < count - 1){
			mdelay(500);
		}
	}
}

///////////////////////// keymatrix functions /////////////////////////
static int keyevent(char key){
	timer_reset();
	
	if(key >= '0' && key <= '9'){
		beep(1, 1);
		if(mode){
			newpass[pos] = key;
			pos++;
		}
		else if(password[pos] == key){
			pos++;
		}
		else{
			pos = 0;
		}
	}
	else if(key == '#'){
		if(mode){
			beep(1, 1);
			newpass[pos] = '#';
			kfree(password);
			password = newpass;
			newpass = NULL;
			mode = 0;

			spin_lock(&event_lock);
			msg = 3;
			spin_unlock(&event_lock);
		}else if(password[pos] == '#'){
			beep(1, 1);
			spin_lock(&event_lock);
			msg = 1;
			spin_unlock(&event_lock);
			motor_action(1);
		}else{
			beep(1, 3);
			spin_lock(&event_lock);
			msg = 2;
			spin_unlock(&event_lock);
		}
		wake_up_interruptible(&wait_queue);
		pos = 0;
	}
	else if(key == 'R'){
		beep(3, 1);
		if(newpass == NULL){
			newpass = (char *)kmalloc(PW_MAX_LENGTH * sizeof(char), GFP_KERNEL);
		}
		mode = 1;
		pos = 0;
	}
	else if(key == 'O'){
		beep(1, 1);
		motor_action(1);
		pos = 0;
	}
	else if(key == 'K'){
		beep(3, 1);
		keep_door_open();
		pos = 0;
	}

	return 0;
}

static int keypad_scan_thread(void * data){
	int i;
	int scandata[4];
	char prev_scan = 0;
	char curt_scan = 0;

	int scan1[4] = {1, 0, 0, 0};
	int scan2[4] = {0, 1, 0, 0};
	int scan3[4] = {0, 0, 1, 0};
	int scan4[4] = {0, 0, 0, 1};
     
	while(!kthread_should_stop()){
		curt_scan = 0;

		for(i = 0 ; i < 4 ; i++){
			gpio_set_value(K_SCAN1, scan1[i]);
			gpio_set_value(K_SCAN2, scan2[i]);
			gpio_set_value(K_SCAN3, scan3[i]);
			gpio_set_value(K_SCAN4, scan4[i]);

			udelay(1);

			scandata[0] = gpio_get_value(K_IN1);
			scandata[1] = gpio_get_value(K_IN2);
			scandata[2] = gpio_get_value(K_IN3);
 			scandata[3] = gpio_get_value(K_IN4);

			if(i == 0){
				if(scandata[0])	curt_scan = '1';
				else if(scandata[1]) curt_scan = '2';
				else if(scandata[2]) curt_scan = '3';
				else if(scandata[3]) curt_scan = 'R';
			}
			else if(i == 1){
				if(scandata[0]) curt_scan = '4';
				else if(scandata[1]) curt_scan = '5';
				else if(scandata[2]) curt_scan = '6';
				else if(scandata[3]) curt_scan = 'O';
			}else if(i == 2){
				if(scandata[0]) curt_scan = '7';
				else if(scandata[1]) curt_scan = '8';
				else if(scandata[2]) curt_scan = '9';
				else if(scandata[3]) curt_scan = 'K';
			}
			else if(i == 3){
				if(scandata[0]) curt_scan = '*';
				else if(scandata[1]) curt_scan = '0';
				else if(scandata[2]) curt_scan = '#';
				else if(scandata[3]) curt_scan = 'B';
			}
			msleep(1);
		}

		if(curt_scan && curt_scan != prev_scan){
			keyevent(curt_scan);
		}

		prev_scan = curt_scan;
	}

	return 0;
}
///////////////////////// motor functions /////////////////////////
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
		setStep(0, 0, 0, 0);
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

		setStep(0, 0, 0, 0);

		door_state = 0;
	}
}

static void motor_action(int oc){
	if(oc){
		if(door_state == 0){
			door_open();
			
			motor_timer.function = door_close;
			motor_timer.data = 0L;
			motor_timer.expires = jiffies + (5 * HZ); //can change
			add_timer(&motor_timer);
		}
	}
	else{
		if(timer_pending(&motor_timer)){
			del_timer(&motor_timer);
		}
		
		door_close(0);	//checks door state inside
	}
}

static void keep_door_open(void){
	if(timer_pending(&motor_timer)){
		del_timer(&motor_timer);
	}
	
	if(door_state == 0){
		door_open();
	}
	
	motor_timer.function = alert_open;
	motor_timer.data = 0L;
	motor_timer.expires = jiffies + (20 * HZ); //can change
	add_timer(&motor_timer);
}

static void alert_open(unsigned long data){
	if(door_state == 1){
		beep(1, 3);
		spin_lock(&event_lock);
		msg = 4;
		spin_unlock(&event_lock);
		wake_up_interruptible(&wait_queue);
	}
}

///////////////////////// etc functions /////////////////////////
static void set_light_state(void) {
	//int brightness = get_value_from_lighe_sensor
	//if brightness is over LIGHT_MIN, light_state = 0;
	//less then LIGHT_MIN, light_state = 1

	int i;
	int data = SPI_REQ_DATA;
	int bright = 0;

	gpio_set_value(LIGHT_EN, 0);

	for (i = 0; i < SPI_CLK_LENGTH; i++){
		gpio_set_value(LIGHT_CLK, 0);
		gpio_set_value(LIGHT_OUT, data & 0x01);
		data >>= 1;
		udelay(SPI_CLK);
		gpio_set_value(LIGHT_CLK, 1);

		if(i > 11){
			bright <<= 1;
			bright |= gpio_get_value(LIGHT_IN) & 0x01;
		}

		udelay(SPI_CLK);
	}

	gpio_set_value(LIGHT_EN, 1);

	light_state = (bright < LIGHT_MIN) ? 1 : 0;
}

static void init_state(unsigned long data) {
	gpio_set_value(LED, 0);
	pos = 0;

	if(newpass){
		kfree(newpass);
		newpass = NULL;
		mode = 0;
	}
}

// when pir detected human
static void timer_reset(void) {
	set_light_state();

	if(light_state){
		gpio_set_value(LED, 1);
	}

	mod_timer(&human_timer, jiffies + TIMER_SEC*HZ);
}

static irqreturn_t pir_isr(int irq, void* dev_id) {
	timer_reset(); 

	return IRQ_HANDLED;
}

///////////////////////// fops init exit /////////////////////////
static ssize_t keypad_read(struct file * file, char * buf, size_t len, loff_t * loff){
	char * buff;
	int msg_len, err;

	if(wait_event_interruptible(wait_queue, msg != 0)){
		return -1;
	}

	spin_lock(&event_lock);
	switch (msg) {
		case 1 :
			buff = "ACCEPT";
			break;
		case 2 : 
			buff = "REJECT";
			break;
		case 3 :
			buff = "CHANGE";
			break;
		case 4 :
			buff = "ALTER";
			break;
		default : 
			buff = "ERROR";
		}
	msg = 0;
	spin_unlock(&event_lock);
	msg_len = strlen(buff);
	err = copy_to_user(buf, buff, msg_len);

	if(err > 0){
		return err;
	}
  
	return msg_len;
}

struct file_operations doorlock_fops = {
	.read = keypad_read
};

static dev_t dev_num;
static struct cdev * cd_cdev;

static int __init doorlock_init(void){
	int ret;
  
	printk("INIT MODULE\n");
  
//gpio request
	gpio_request_array(dev_gpios, sizeof(dev_gpios)/sizeof(struct gpio));

//chr dev init
	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &doorlock_fops);
	cdev_add(cd_cdev, dev_num, 1);

//init password
	password = (char *)kmalloc(PW_MAX_LENGTH * sizeof(char), GFP_KERNEL);

	init_waitqueue_head(&wait_queue);
  
	init_timer(&motor_timer);
	init_timer(&human_timer); 
	human_timer.function = init_state;
  
//init keypad thread
	keypad_task = kthread_create(keypad_scan_thread, NULL, "keypad_scan_thread");
	if(IS_ERR(keypad_task)){
		keypad_task = NULL;
		printk("KEYPAD_SCAN_TASK CREATE ERROR\n");
	}
	wake_up_process(keypad_task);

//init irq
	irq_pir = gpio_to_irq(PIR);

	ret = request_irq(irq_pir, pir_isr, IRQF_TRIGGER_RISING, "pir_irq", NULL);
	if(ret) {
		printk("request irq err %d\n", ret);
		free_irq(irq_pir, NULL);
	}

	return 0;
}

static void __exit doorlock_exit(void){
	printk("EXIT MODULE\n");

//chr dev exit 
	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num, 1);

//gpio free
	gpio_set_value(SPEAKER, 0);
	gpio_free_array(dev_gpios, sizeof(dev_gpios)/sizeof(struct gpio));

//free irq
	free_irq(irq_pir, NULL);
  
//del timer
	del_timer(&motor_timer);
	del_timer(&human_timer);
  
	if(keypad_task){
		kthread_stop(keypad_task);
	}
}

module_init(doorlock_init);
module_exit(doorlock_exit);
