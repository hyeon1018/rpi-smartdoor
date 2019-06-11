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
#define LED 16
#define PIR 20
#define LIGHT_CLK 13
#define LIGHT_IN 6
#define LIGHT_OUT 5
#define LIGHT_EN 24

#define LIGHT_MIN 500           // min value of light sensor for LED on
#define TIMER_SEC 10            // time of light lasts on

#define SPI_CLK_LENGTH 24
#define SPI_REQ_DATA 0x60
#define SPI_CLK 10

#define DEV_NAME "doorlock_dev"

MODULE_LICENSE("GPL");

///////////////////////// key matrix /////////////////////////


///////////////////////// motor /////////////////////////


///////////////////////// etc /////////////////////////

