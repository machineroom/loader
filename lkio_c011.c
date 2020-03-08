
#include "LKIO.H"
#include <assert.h>
#include <stdio.h>
#include "c011.h"

#define TIMEOUT 5000

void rst_adpt(int p) {
    c011_reset();
}

int init_lkio(int p,int q,int r) {
    c011_init();
    return 0;
}

//return 0==fail, 1==OK
int tbyte_out(int p) {
    if (c011_write_byte ((uint8_t)p, TIMEOUT) == 0) {    
        return 1;
    } else {
        return 0;
    }    
}

long word_in(void) {
    uint8_t b1;     //MS byte
    uint8_t b2;
    uint8_t b3;
    uint8_t b4;     //LS byte
    c011_read_byte (&b4, TIMEOUT);
    c011_read_byte (&b3, TIMEOUT);
    c011_read_byte (&b2, TIMEOUT);
    c011_read_byte (&b1, TIMEOUT);
    long ret=0;
    ret |= b1;
    ret <<= 8;
    ret |= b2;
    ret <<= 8;
    ret |= b3;
    ret <<= 8;
    ret |= b4;
    //printf ("word in 0x%02X 0x%02X 0x%02X 0x%02X = 0x%X\n",b1,b2,b3,b4,ret);
    return ret;
}

void word_out(long p) {
    uint8_t b1 = p>>24;     //MS byte
    uint8_t b2 = p>>16;
    uint8_t b3 = p>>8;
    uint8_t b4 = p&0xFF;    //LS byte
    c011_write_byte (b4, TIMEOUT);
    c011_write_byte (b3, TIMEOUT);
    c011_write_byte (b2, TIMEOUT);
    c011_write_byte (b1, TIMEOUT);
}

void chan_in(char *p,unsigned int count) {
    int ret = c011_read_bytes ((uint8_t *)p, count, TIMEOUT);
    if (ret != count) {
        fprintf (stderr,"*E* failed to read bytes: %d != %d\n", ret, count);
    }
}

void chan_out(char *p,unsigned int count) {
    int ret = c011_write_bytes ((uint8_t *)p, count, TIMEOUT);
    if (ret != count) {
        fprintf (stderr,"*E* failed to write bytes: %d != %d\n", ret, count);
    }
}

void byte_out(int p) {
    //not used
    assert (0);
}

int  err_flag(void) {
    //not used
    assert (0);
}

int  busy_in(void) {
    //not used
    assert (0);
}

int  busy_out(int p) {
    //not used
    assert (0);
}

int  byte_in(void) {
    //not used
    assert (0);
}

int  tbyte_in(void) {
    //not used
    assert (0);
}

void dma_in(char *p,unsigned int q) {
    //not used
    assert (0);
}

void dma_out(char *p,unsigned int q) {
    //not used
    assert (0);
}

void dma_on(void) {
    //not used
    assert (0);
}

void dma_off(void) {
    //not used
    assert (0);
}


