
/*
 *  Function declarations for the Link I/O interface to the link adaptor.
 *  Copyright (C) Computer System Architects, 1988
 */
#include "conc_native.h"

static Channel *host_in;
static Channel *host_out;

// return 0 on OK, -1 on error
int word_in(int *word) {
    *word = ChanInInt(host_in);
    return 0;
}

// return 0 on OK, -1 on error
int word_out(int word) {
    ChanOutInt (host_out, word);
    return 0;
}

// return 0 on OK, -1 on error
int chan_in(char *p, unsigned int count) {
    ChanIn (host_in, p, count);
    return 0;
}

// return 0 on OK, -1 on error
int chan_out(char *p, unsigned int count) {
    ChanOut (host_out, p, count);
    return 0;
}

void rst_adpt(void) {

}

// return 0 on OK, -1 on error
int init_lkio() {
    host_in = ChanAlloc();
    host_out = ChanAlloc();
    return 0;
}

void *get_host_in(void) {
    return host_in;
}

void *get_host_out(void) {
    return host_out;
}
