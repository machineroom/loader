
#include "LKIO.H"
#include <assert.h>
#include <stdio.h>
#include "c011.h"

#define TIMEOUT 5000

void rst_adpt() {
    c011_reset();
}

int init_lkio() {
    c011_init();
    return 0;
}

//return 0==OK, -1 error
int tbyte_out(int p) {
    return c011_write_byte ((uint8_t)p, TIMEOUT);
}

//return 0==OK, -1 error
int word_in(int *word) {
    int stat;
    uint8_t b1;     //MS byte
    uint8_t b2;
    uint8_t b3;
    uint8_t b4;     //LS byte
    stat = c011_read_byte (&b4, TIMEOUT);
    if (stat) return stat;
    stat = c011_read_byte (&b3, TIMEOUT);
    if (stat) return stat;
    stat = c011_read_byte (&b2, TIMEOUT);
    if (stat) return stat;
    stat = c011_read_byte (&b1, TIMEOUT);
    if (stat) return stat;
    long ret=0;
    ret |= b1;
    ret <<= 8;
    ret |= b2;
    ret <<= 8;
    ret |= b3;
    ret <<= 8;
    ret |= b4;
    //printf ("word in 0x%02X 0x%02X 0x%02X 0x%02X = 0x%X\n",b1,b2,b3,b4,ret);
    *word = ret;
    return 0;
}

//return 0==OK, -1 error
int word_out(int p) {
    int stat;
    uint8_t b1 = p>>24;     //MS byte
    uint8_t b2 = p>>16;
    uint8_t b3 = p>>8;
    uint8_t b4 = p&0xFF;    //LS byte
    stat = c011_write_byte (b4, TIMEOUT);
    if (stat) return stat;
    stat = c011_write_byte (b3, TIMEOUT);
    if (stat) return stat;
    stat = c011_write_byte (b2, TIMEOUT);
    if (stat) return stat;
    stat = c011_write_byte (b1, TIMEOUT);
    if (stat) return stat;
    return 0;
}

int chan_in(char *p, unsigned int count) {
    uint32_t done = c011_read_bytes ((uint8_t *)p, count, TIMEOUT);
    if (done==count) return 0;
    else return -1;
}

int chan_out(char *p, unsigned int count) {
    uint32_t done = c011_write_bytes ((uint8_t *)p, count, TIMEOUT);
    if (done==count) return 0;
    else return -1;
}
