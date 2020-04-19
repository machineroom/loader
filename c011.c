#include <bcm2835.h>
#include <time.h>
#include "pins.h"
#include "c011.h"
#include <stdio.h>


#define TCSLCSH (60)
#define TCSHCSL (50)
#define TCSLDrV (50)

static uint32_t bits=0;
static volatile uint32_t *gpio_clr;
static volatile uint32_t *gpio_set;
static volatile uint32_t *gpio_fsel;
static volatile uint32_t *gpio_lev;

static uint64_t total_reads=0;
static uint64_t total_read_waits=0;
static uint64_t total_read_timeouts=0;
static uint64_t total_read_success=0;

static uint64_t total_writes=0;
static uint64_t total_write_waits=0;
static uint64_t total_write_timeouts=0;
static uint64_t total_write_success=0;

void c011_dump_stats(char *title) {
    printf ("C011 interfaces stats for '%s'\n",title);
    printf ("\ttotal reads %llu\n",total_reads);
    printf ("\ttotal read waits %llu\n",total_read_waits);
    printf ("\ttotal read timouts %llu\n",total_read_timeouts);
    printf ("\ttotal read successes %llu\n",total_read_success);
    printf ("\ttotal writes %llu\n",total_writes);
    printf ("\ttotal write waits %llu\n",total_write_waits);
    printf ("\ttotal write timouts %llu\n",total_write_timeouts);
    printf ("\ttotal write successes %llu\n",total_write_success);
    total_reads=0;
    total_read_waits=0;
    total_read_timeouts=0;
    total_read_success=0;
    total_writes=0;
    total_write_waits=0;
    total_write_timeouts=0;
    total_write_success=0;
}

static inline void sleep_ns(int ns) {
    struct timespec s = {0,ns};
    int ret = nanosleep(&s,NULL);
    if (ret != 0) {
        printf ("nanosleep(%d) failed\n", ret);
    }
}

static void set_control_output_pins(void) {
    bcm2835_gpio_fsel(RS0, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(RS1, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(RESET, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(CS, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(RW, BCM2835_GPIO_FSEL_OUTP);
}

static inline void set_data_output_pins(void) {
    //bits 9-0 output (001)
    //%00001001001001001001001001001001
    //    0   9   2   4   9   2   4   9
    bcm2835_peri_write (gpio_fsel, 0x09249249);
}

static inline void set_data_input_pins(void) {
    //bits 9-0 input (000)
    bcm2835_peri_write (gpio_fsel, 0);
}

static inline void set_gpio_bit(uint8_t pin, uint8_t on) {
    if (on) {
        bits |= 1<<pin;
    } else {
        bits &= ~(1<<pin);
    }
}

static inline void gpio_commit(void) {
    bcm2835_peri_write (gpio_clr, ~bits);
    bcm2835_peri_write (gpio_set, bits);
}
        
void c011_init(void) {
    bcm2835_init();
    gpio_clr = bcm2835_regbase(BCM2835_REGBASE_GPIO) + BCM2835_GPCLR0/4;
    gpio_set = bcm2835_regbase(BCM2835_REGBASE_GPIO) + BCM2835_GPSET0/4;
    gpio_fsel = bcm2835_regbase(BCM2835_REGBASE_GPIO) + BCM2835_GPFSEL0/4;
    gpio_lev = bcm2835_regbase(BCM2835_REGBASE_GPIO) + BCM2835_GPLEV0/4;
    set_control_output_pins();
    set_gpio_bit(ANALYSE, LOW);
    gpio_commit();
}

void c011_reset(void) {
    set_gpio_bit(ANALYSE, LOW);
    gpio_commit();
    //TN29 states "Recommended pulse width is 5 ms, with a delay of 5 ms before sending anything down a link."
    set_gpio_bit(RESET, LOW);
    gpio_commit();
    set_gpio_bit(RESET, HIGH);
    gpio_commit();
    bcm2835_delayMicroseconds (5*1000);
    set_gpio_bit(RESET, LOW);
    gpio_commit();
    bcm2835_delayMicroseconds (5*1000);
    //The whitecross HSL takes some time to cascade reset
    bcm2835_delay(1000);
}

void c011_analyse(void) {
    set_gpio_bit(ANALYSE, LOW);
    gpio_commit();
    bcm2835_delayMicroseconds (5*1000);
    set_gpio_bit(ANALYSE, HIGH);
    gpio_commit();
    bcm2835_delayMicroseconds (5*1000);
    set_gpio_bit(RESET, HIGH);
    gpio_commit();
    bcm2835_delayMicroseconds (5*1000);
    set_gpio_bit(RESET, LOW);
    gpio_commit();
    bcm2835_delayMicroseconds (5*1000);
    set_gpio_bit(ANALYSE, LOW);
    gpio_commit();
    bcm2835_delayMicroseconds (5*1000);
}

int c011_write_byte(uint8_t byte, uint32_t timeout) {
    //wait for output ready
    uint64_t timeout_us = timeout*1000;
    uint32_t word;
    total_writes++;
    while ((c011_read_output_status() & 0x01) != 0x01 && timeout_us>0) {
        bcm2835_delayMicroseconds(1);
        timeout_us--;
        total_write_waits++;
    }
    if (timeout_us == 0) {
        total_write_timeouts++;
        return -1;
    }
    //RS1=0, RS0=1
    //RW=0
    //CS=1
    set_data_output_pins();
    set_gpio_bit (RS1,0);
    set_gpio_bit (RS0,1);
    set_gpio_bit (RW,0);
    set_gpio_bit (CS,1);
    gpio_commit();
    // 1111 1111 1111 1111   1111 1100 0000 0011
    //clear bits 2-9 and OR in our byte
    bits &= 0xFFFFFC03;
    word = byte;
    word <<= 2;
    bits |= word;
    //CS=0
    set_gpio_bit(CS, LOW);
    gpio_commit();
    sleep_ns (TCSLCSH);
    //CS=1
    set_gpio_bit(CS, HIGH);
    gpio_commit();
    sleep_ns (TCSHCSL);
    total_write_success++;
    return 0;
}

static uint8_t read_c011(void) {
    set_data_input_pins();
    set_gpio_bit(CS, LOW);
    gpio_commit();
    //must allow time for data valid after !CS
    sleep_ns (TCSLDrV);
    uint32_t reg = bcm2835_peri_read (gpio_lev);
    uint8_t byte;
    reg >>= 2;
    byte = reg & 0xFF;
    set_gpio_bit(CS, HIGH);
    gpio_commit();
    sleep_ns (TCSHCSL);
    return byte;
}

uint8_t c011_read_input_status(void) {
    uint8_t byte;
    set_gpio_bit (RS1,1);
    set_gpio_bit (RS0,0);
    set_gpio_bit (RW,1);
    set_gpio_bit (CS,1);
    gpio_commit();
    byte = read_c011();
    return byte;
}

uint8_t c011_read_output_status(void) {
    uint8_t byte;
    set_gpio_bit (RS1,1);
    set_gpio_bit (RS0,1);
    set_gpio_bit (RW,1);
    set_gpio_bit (CS,1);
    gpio_commit();
    byte = read_c011();
    return byte;
}

int c011_read_byte(uint8_t *byte, uint32_t timeout) {
    uint64_t timeout_us = timeout*1000;
    total_reads++;
    if (timeout==0) {
        while ((c011_read_input_status() & 0x01) == 0x00) {
            total_read_waits++;
            sleep_ns(1);
        }
    } else {
        while ((c011_read_input_status() & 0x01) == 0x00 && timeout_us>0) {
            bcm2835_delayMicroseconds(1);
            timeout_us--;
            total_read_waits++;
        }
        if (timeout_us == 0) {
            total_read_timeouts++;
            return -1;
        }
    }
    set_gpio_bit (RS1,0);
    set_gpio_bit (RS0,0);
    set_gpio_bit (RW,1);
    set_gpio_bit (CS,1);
    gpio_commit();
    *byte = read_c011();
    total_read_success++;
    return 0;
}

uint32_t c011_write_bytes (uint8_t *bytes, uint32_t num, uint32_t timeout) {
    uint32_t i;
    for (i=0; i < num; i++) {
        int ret = c011_write_byte(bytes[i], timeout);
        if (ret == -1) {
            break;
        }
    }
    return i;
}

uint32_t c011_read_bytes (uint8_t *bytes, uint32_t num, uint32_t timeout) {
    uint32_t i;
    for (i=0; i < num; i++) {
        int ret = c011_read_byte(&bytes[i], timeout);
        if (ret == -1) {
            break;
        }
    }
    return i;
}

