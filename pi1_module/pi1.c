#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
//#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/delay.h>


// GPIO PIN number
#define LED 16                  
#define PIR 20                  
#define LIGHT_CLK 13
#define LIGHT_IN 6
#define LIGHT_OUT 5
#define LIGHT_EN 24

//#define DEV_NAME "pi1_dev"

#define LIGHT_MIN 500           // min value of light sensor for LED on
#define TIMER_SEC 10            // time of light lasts on

#define SPI_CLK_LENGTH 24
#define SPI_REQ_DATA 0x60
#define SPI_CLK 10

MODULE_LICENSE("GPL");



/* TODO
receive keymat_state from pi2
send pir_state to pi2
*/



static int irq_pir;
//pir is 1 when deteced, light is 1 when light has to turn on, keymat is 1 when detected 
static int pir_state, light_state, keymat_state = 0;
static struct timer_list led_timer;

static void led_timer_expired(unsigned long data);

static struct gpio pi1_gpios[6] = {{LED, GPIOF_OUT_INIT_LOW, "led"},
                              {PIR, GPIOF_IN, "pir"},
                              {LIGHT_CLK, GPIOF_INIT_LOW, "light_clk"},
                              {LIGHT_IN, GPIOF_IN, "light_in"},
                              {LIGHT_OUT, GPIOF_INIT_LOW, "light_data_out"},
                              {LIGHT_EN, GPIOF_INIT_HIGH, "light_enable"}};


//if request failed, consider keymat_state is 0
static void request_keymat_state(void) {
    //int received vaule = get_vaule_from_pi2
    //if received value is valid, keymat_state = received value
    //if received value is invalid(failed), keymat_state is 0
}

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

static void init_state(void) {
    pir_state = 0;
    light_state = 0;
    keymat_state = 0;
    gpio_set_value(LED, 0);
}

// when pir detected human
static void led_timer_reset(void) {
    printk("reset timer\n");

    set_light_state();
    if((pir_state || keymat_state) && light_state) gpio_set_value(LED, 1);

    if(gpio_get_value(LED)) del_timer(&led_timer);
    led_timer.expires = get_jiffies_64() + TIMER_SEC*HZ;
    led_timer.function = led_timer_expired;
    add_timer(&led_timer);
}

// when pir didn't detect human
static void led_timer_expired(unsigned long data) {
    pir_state = 0;
    request_keymat_state();

    if(keymat_state) {
        led_timer_reset();
    } else {
        init_state();
    }
}

static irqreturn_t pir_isr(int irq, void* dev_id) {
    printk("pir detected, %d\n", gpio_get_value(PIR));
    pir_state = 1;
    led_timer_reset(); 

    return IRQ_HANDLED;
}




////////////////////////////////////////////////////////////////////////////////


/*
static int pi1_open(struct inode *inode, struct file *file) {
    printk("PI1 OPEN\n");

    enable_irq(irq_pir);

    return 0;
}

static int pi1_release(struct inode *inode, struct file *file) {
    printk("pi1 RELEASE\n");

    disable_irq(irq_pir);

    return 0;
}

struct file_operations fops = {
    .open = pi1_open,
    .release = pi1_release
};
*/



////////////////////////////////////////////////////////////////////////////////


//static dev_t dev_num;
//static struct cdev *cd_cdev;

static int __init pi1_init(void) {
    int r;

    printk("PI1 INIT\n");

    /*
    alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
    cd_cdev = cdev_alloc();
    cdev_init(cd_cdev, NULL);
    cdev_add(cd_cdev, dev_num, 1);
    */

    init_timer(&led_timer);    

    gpio_request_array(pi1_gpios, sizeof(pi1_gpios)/sizeof(struct gpio));

    irq_pir = gpio_to_irq(PIR);
    r = request_irq(irq_pir, pir_isr, IRQF_TRIGGER_FALLING, "pir_irq", NULL);
    if(r) {
        printk("request irq err %d\n", r);
        free_irq(irq_pir, NULL);
    } else {
        disable_irq(irq_pir);
    }

    enable_irq(irq_pir);

    return 0;
}

static void __exit pi1_exit(void) {
    printk("PI1 EXIT\n");

    /*
    cdev_del(cd_cdev);
    unregister_chrdev_region(dev_num, 1);
    */

    free_irq(irq_pir, NULL);
    gpio_free_array(pi1_gpios, sizeof(pi1_gpios)/sizeof(struct gpio));
}

module_init(pi1_init);
module_exit(pi1_exit);



