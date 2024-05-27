
/*
 *  Function declarations for the Link I/O interface to the link adaptor.
 *  Copyright (C) Computer System Architects, 1988
 */
#include "conc_native.h"

static Channel *host_channel;

// return 0 on OK, -1 on error
int word_in(int *word) {
    *word = ChanInInt(host_channel);
    return 0;
}

// return 0 on OK, -1 on error
int word_out(int word) {
    ChanOutInt (host_channel, word);
    return 0;
}

// return 0 on OK, -1 on error
int chan_in(char *p, unsigned int count) {
    ChanIn (host_channel, p, count);
    return 0;
}

// return 0 on OK, -1 on error
int chan_out(char *p, unsigned int count) {
    ChanOut (host_channel, p, count);
    return 0;
}

void rst_adpt(void) {

}

// return 0 on OK, -1 on error
int init_lkio() {
    host_channel = ChanAlloc();
    return 0;
}

void *get_host_channel(void) {
    return host_channel;
}
