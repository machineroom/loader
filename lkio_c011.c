
#include "LKIO.H"

int  err_flag(void) {}
int  busy_in(void) {}
int  busy_out(int p) {}
int  byte_in(void) {}
void byte_out(int p) {}
int  tbyte_in(void) {}
int  tbyte_out(int p) {}
long word_in(void) {}
void word_out(long p) {}
void chan_in(char *p,unsigned int q) {}
void chan_out(char *p,unsigned int q) {}
void dma_in(char *p,unsigned int q) {}
void dma_out(char *p,unsigned int q) {}
void dma_on(void) {
}
void dma_off(void) {
}
void rst_adpt(int p) {
}
int  init_lkio(int p,int q,int r) {
}
