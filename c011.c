#include <bcm2835.h>
#include <time.h>
#include "pins.h"
#include "c011.h"
#include <stdio.h>


#define TCSLCSH (60)
#define TCSHCSL (50)
#define TCSLDrV (50)

static void sleep_ns(int ns) {
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

static void set_data_output_pins(void) {
    bcm2835_gpio_fsel(D0, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(D1, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(D2, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(D3, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(D4, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(D5, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(D6, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(D7, BCM2835_GPIO_FSEL_OUTP);
}

static void set_data_input_pins(void) {
    bcm2835_gpio_fsel(D0, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(D1, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(D2, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(D3, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(D4, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(D5, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(D6, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(D7, BCM2835_GPIO_FSEL_INPT);
}
        
void c011_init(void) {
    bcm2835_init();
    set_control_output_pins();
    bcm2835_gpio_write(ANALYSE, LOW);
}

void c011_reset(void) {
    bcm2835_gpio_write(ANALYSE, LOW);
    //TN29 states "Recommended pulse width is 5 ms, with a delay of 5 ms before sending anything down a link."
    bcm2835_gpio_write(RESET, LOW);
    bcm2835_gpio_write(RESET, HIGH);
    bcm2835_delayMicroseconds (5*1000);
    bcm2835_gpio_write(RESET, LOW);
    bcm2835_delayMicroseconds (5*1000);
    //The whitecross HSL takes some time to cascade reset
    bcm2835_delay(1000);
}

void c011_analyse(void) {
    bcm2835_gpio_write(ANALYSE, LOW);
    bcm2835_delayMicroseconds (5*1000);
    bcm2835_gpio_write(ANALYSE, HIGH);
    bcm2835_delayMicroseconds (5*1000);
    bcm2835_gpio_write(RESET, HIGH);
    bcm2835_delayMicroseconds (5*1000);
    bcm2835_gpio_write(RESET, LOW);
    bcm2835_delayMicroseconds (5*1000);
    bcm2835_gpio_write(ANALYSE, LOW);
    bcm2835_delayMicroseconds (5*1000);
}

void c011_enable_out_int(void) {
    set_data_output_pins();
    bcm2835_gpio_write_mask (1<<RS1 | 1<<RS0 | 0<<RW | 1<<CS,
                             1<<RS1 | 1<<RS0 | 1<<RW | 1<<CS);
    bcm2835_gpio_write_mask (1<<D1,
                             1<<D7 | 1<<D6 | 1<<D5 | 1<<D4 | 1<<D3 | 1<<D2 | 1<<D1 | 1<<D0);

    bcm2835_gpio_write(CS, LOW);
    sleep_ns (TCSLCSH);
    bcm2835_gpio_write(CS, HIGH);
    sleep_ns (TCSHCSL);
}

void c011_enable_in_int(void) {
    set_data_output_pins();
    bcm2835_gpio_write_mask (1<<RS1 | 0<<RS0 | 0<<RW | 1<<CS,
                             1<<RS1 | 1<<RS0 | 1<<RW | 1<<CS);
    bcm2835_gpio_write_mask (1<<D1,
                             1<<D7 | 1<<D6 | 1<<D5 | 1<<D4 | 1<<D3 | 1<<D2 | 1<<D1 | 1<<D0);
    bcm2835_gpio_write(CS, LOW);
    sleep_ns (TCSLCSH);
    bcm2835_gpio_write(CS, HIGH);
    sleep_ns (TCSHCSL);
}

int c011_write_byte(uint8_t byte, uint32_t timeout) {
    //wait for output ready
    while ((c011_read_output_status() & 0x01) != 0x01 && timeout>0) {
        bcm2835_delayMicroseconds(1000);
        timeout--;
    }
    if (timeout == 0) {
        return -1;
    }
    //RS1=0, RS0=1
    //RW=0
    //CS=1
    set_data_output_pins();
    bcm2835_gpio_write_mask (0<<RS1 | 1<<RS0 | 0<<RW | 1<<CS,
                             1<<RS1 | 1<<RS0 | 1<<RW | 1<<CS);
    //D0-D7
    uint8_t d7,d6,d5,d4,d3,d2,d1,d0;
    d7=(byte&0x80) >> 7;
    d6=(byte&0x40) >> 6;
    d5=(byte&0x20) >> 5;
    d4=(byte&0x10) >> 4;
    d3=(byte&0x08) >> 3;
    d2=(byte&0x04) >> 2;
    d1=(byte&0x02) >> 1;
    d0=(byte&0x01) >> 0;
    bcm2835_gpio_write_mask (d7<<D7 | d6<<D6 | d5<<D5 | d4<<D4 | d3<<D3 | d2<<D2 | d1<<D1 | d0<<D0,
                             1<<D7 | 1<<D6 | 1<<D5 | 1<<D4 | 1<<D3 | 1<<D2 | 1<<D1 | 1<<D0);
    //CS=0
    bcm2835_gpio_write(CS, LOW);
    sleep_ns (TCSLCSH);
    //CS=1
    bcm2835_gpio_write(CS, HIGH);
    sleep_ns (TCSHCSL);
    return 0;
}

static uint8_t read_c011(void) {
    uint8_t d7,d6,d5,d4,d3,d2,d1,d0,byte;
    set_data_input_pins();
    bcm2835_gpio_write(CS, LOW);
    //must allow time for data valid after !CS
    sleep_ns (TCSLDrV);
    uint32_t reg = bcm2835_peri_read (bcm2835_regbase(BCM2835_REGBASE_GPIO) + BCM2835_GPLEV0/4);
    d7=(reg & 1<<D7) != 0;
    d6=(reg & 1<<D6) != 0;
    d5=(reg & 1<<D5) != 0;
    d4=(reg & 1<<D4) != 0;
    d3=(reg & 1<<D3) != 0;
    d2=(reg & 1<<D2) != 0;
    d1=(reg & 1<<D1) != 0;
    d0=(reg & 1<<D0) != 0;
    byte = d7;
    byte <<= 1;
    byte |= d6;
    byte <<= 1;
    byte |= d5;
    byte <<= 1;
    byte |= d4;
    byte <<= 1;
    byte |= d3;
    byte <<= 1;
    byte |= d2;
    byte <<= 1;
    byte |= d1;
    byte <<= 1;
    byte |= d0;
    bcm2835_gpio_write(CS, HIGH);
    sleep_ns (TCSHCSL);
    return byte;
}

uint8_t c011_read_input_status(void) {
    uint8_t byte;
    bcm2835_gpio_write_mask (1<<RS1 | 0<<RS0 | 1<<RW | 1<<CS,
                             1<<RS1 | 1<<RS0 | 1<<RW | 1<<CS);
    byte = read_c011();
    return byte;
}

uint8_t c011_read_output_status(void) {
    uint8_t byte;
    bcm2835_gpio_write_mask (1<<RS1 | 1<<RS0 | 1<<RW | 1<<CS,
                             1<<RS1 | 1<<RS0 | 1<<RW | 1<<CS);
    byte = read_c011();
    return byte;
}

int c011_read_byte(uint8_t *byte, uint32_t timeout) {
    while ((c011_read_input_status() & 0x01) == 0x00 && timeout>0) {
        bcm2835_delayMicroseconds(1000);
        timeout--;
    }
    if (timeout == 0) {
        return -1;
    }
    bcm2835_gpio_write_mask (0<<RS1 | 0<<RS0 | 1<<RW | 1<<CS,
                             1<<RS1 | 1<<RS0 | 1<<RW | 1<<CS);
    *byte = read_c011();
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

