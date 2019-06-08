#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#define K_SCAN1 6
#define K_SCAN2 13
#define K_SCAN3 19
#define K_SCAN4 26
#define K_IN1 12
#define K_IN2 16
#define K_IN3 20
#define K_IN4 21

#define DEV_NAME "keypad_dev"

spinlock_t input_lock;
static char * input;
static char * data;
wait_queue_head_t wait_queue;
struct task_struct * keypad_task = NULL;


static dev_t dev_num;
static struct cdev * cd_cdev;

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
                if(scandata[0]){
                    curt_scan = '1';
                }else if(scandata[1]){
                    curt_scan = '2';
                }else if(scandata[2]){
                    curt_scan = '3';
                }else if(scandata[3]){
                    curt_scan = 'R';
                }
            }else if(i == 1){
                if(scandata[0]){
                    curt_scan = '4';
                }else if(scandata[1]){
                    curt_scan = '5';
                }else if(scandata[2]){
                    curt_scan = '6';
                }else if(scandata[3]){
                    curt_scan = 'O';
                }
            }else if(i == 2){
                if(scandata[0]){
                    curt_scan = '7';
                }else if(scandata[1]){
                    curt_scan = '8';
                }else if(scandata[2]){
                    curt_scan = '9';
                }else if(scandata[3]){
                    curt_scan = 'A';
                }
            }else if(i == 3){
                if(scandata[0]){
                    curt_scan = '*';
                }else if(scandata[1]){
                    curt_scan = '0';
                }else if(scandata[2]){
                    curt_scan = '#';
                }else if(scandata[3]){
                    curt_scan = 'B';
                }
            }

            msleep(1);
        }

        if(curt_scan && curt_scan != prev_scan){
            printk("KEY : %c\n", curt_scan);
        }
        
        prev_scan = curt_scan;
    }

    return 0;
}

static int __init keypad_init(void){
    int r;
    //gpio request.
    gpio_request_one(K_SCAN1, GPIOF_OUT_INIT_LOW, "key_scan_1");
    gpio_request_one(K_SCAN2, GPIOF_OUT_INIT_LOW, "key_scan_2");
    gpio_request_one(K_SCAN3, GPIOF_OUT_INIT_LOW, "key_scan_3");
    gpio_request_one(K_SCAN4, GPIOF_OUT_INIT_LOW, "key_scan_4");

    gpio_request_one(K_IN1, GPIOF_IN, "key_in_1");
    gpio_request_one(K_IN2, GPIOF_IN, "key_in_2");
    gpio_request_one(K_IN3, GPIOF_IN, "key_in_3");
    gpio_request_one(K_IN4, GPIOF_IN, "key_in_4");

    //init password.
    printk("init module \n");

    keypad_task = kthread_create(keypad_scan_thread, NULL, "keypad_scan_thread");
    if(IS_ERR(keypad_task)){
        keypad_task = NULL;
        printk("KEYPAD_SCAN_TASK CREATE ERROR\n");
    }

    wake_up_process(keypad_task);

    spin_lock_init(&input_lock);
    return 0;
}

static void __exit keypad_exit(void){

    gpio_free(K_SCAN1);
    gpio_free(K_SCAN2);
    gpio_free(K_SCAN3);
    gpio_free(K_SCAN4);

    gpio_free(K_IN1);
    gpio_free(K_IN2);
    gpio_free(K_IN3);
    gpio_free(K_IN4);

    if(keypad_task){
        kthread_stop(keypad_task);
    }
}

MODULE_LICENSE("GPL");
module_init(keypad_init);
module_exit(keypad_exit);