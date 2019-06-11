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
#define LED 16          // k_in2 overlap
#define PIR 20           //k_in3 overlap
#define LIGHT_CLK 13    //k_scan2 overlap
#define LIGHT_IN 6      //k_scan1 overlap
#define LIGHT_OUT 5
#define LIGHT_EN 24

#define LIGHT_MIN 500           // min value of light sensor for LED on
#define TIMER_SEC 10            // time of light lasts on

#define SPI_CLK_LENGTH 24
#define SPI_REQ_DATA 0x60
#define SPI_CLK 10

#define DEV_NAME "doorlock_dev"
/*
#define IOCTL_START_NUM 0x80
#define IOCTL_MOTOR IOCTL_START_NUM+1
#define IOCTL_FORCE IOCTL_START_NUM+2
#define IOCTL_NUM 'z'
#define MOTOR _IOWR(SIMPLE_IOCTL_NUM, IOCTL_MOTOR, unsigned long *)
#define FORCE _IOWR(SIMPLE_IOCTL_NUM, IOCTL_FORCE, unsigned long *)
*/

MODULE_LICENSE("GPL");

///////////////////////// declare key matrix /////////////////////////
spinlock_t event_lock;
wait_queue_head_t wait_queue;
struct task_struct * keypad_task = NULL;
static struct timer_list reset_timer;

char * password = NULL;
char * newpass = NULL;
int msg = 0; // 1 ACCEPT, 2 REJECT;
int pos = 0;
int mode = 0; // 1 when change password.

///////////////////////// declare motor /////////////////////////
static int door_state = 0; //0:closed 1: opened
static struct timer_list motor_timer;

///////////////////////// declare etc /////////////////////////
/* TODO
receive keymat_state from pi2
send pir_state to pi2
*/

static int irq_pir;
//pir is 1 when deteced, light is 1 when light has to turn on, keymat is 1 when detected 
static int pir_state, light_state, keymat_state = 0;
static struct timer_list led_timer;

static void led_timer_expired(unsigned long data);

///////////////////////// gpios /////////////////////////
static struct gpio dev_gpios[18] = {{LED, GPIOF_OUT_INIT_LOW, "led"},
                               {PIR, GPIOF_IN, "pir"},
                               {LIGHT_CLK, GPIOF_INIT_LOW, "light_clk"},
                               {LIGHT_IN, GPIOF_IN, "light_in"},
                               {LIGHT_OUT, GPIOF_INIT_LOW, "light_data_out"},
                               {LIGHT_EN, GPIOF_INIT_HIGH, "light_enable"},
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


///////////////////////// keymatrix functions /////////////////////////

///////////////////////// motor functions /////////////////////////

///////////////////////// etc functions /////////////////////////


///////////////////////// ioctl init exit /////////////////////////
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

static int dev_open(struct inode *inode, struct file *file) {
    printk("MODULE OPEN\n");
    enable_irq(irq_pir);
  
    return 0;
}
static int dev_release(struct inode *inode, struct file *file) {
    printk("MODULE RELEASE\n");
    disable_irq(irq_pir);
  
    return 0;
}

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

struct file_operations doorlock_fops = {
    .unlocked_ioctl = door_ioctl,
    .read = keypad_read,
    .open = dev_open,
    .release = dev_release
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
  
//init timer
    init_timer(&reset_timer);
    reset_timer.function = reset_pos_func;
    reset_timer.data = 0L;
  
    init_timer(&motor_timer);
    init_timer(&led_timer); 
  
//init keypad thread
    keypad_task = kthread_create(keypad_scan_thread, NULL, "keypad_scan_thread");
    if(IS_ERR(keypad_task)){
        keypad_task = NULL;
        printk("KEYPAD_SCAN_TASK CREATE ERROR\n");
    }
    wake_up_process(keypad_task);

//init irq
    irq_pir = gpio_to_irq(PIR);
    ret = request_irq(irq_pir, pir_isr, IRQF_TRIGGER_FALLING, "pir_irq", NULL);
    if(ret) {
        printk("request irq err %d\n", ret);
        free_irq(irq_pir, NULL);
    } else {
        disable_irq(irq_pir);
    }

    enable_irq(irq_pir);
  
    return 0;
}

static void __exit doorlock_exit(void){
    printk("EXIT MODULE\n");

//chr dev exit 
    cdev_del(cd_cdev);
    unregister_chrdev_region(dev_num, 1);

//gpio free
    gpio_free_array(dev_gpios, sizeof(dev_gpios)/sizeof(struct gpio));

//free irq
    free_irq(irq_pir, NULL);
  
//del timer
    del_timer(&reset_timer);
    del_timer(&motor_timer);
    del_timer(&led_timer);
  
    if(keypad_task){
        kthread_stop(keypad_task);
    }
}

module_init(doorlock_init);
module_exit(doorlock_exit);
