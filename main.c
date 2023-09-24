
#include <string.h>
#include <stdio.h>
#include "LKIO.H"
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <gflags/gflags.h>
#include <termios.h>
#include <sys/stat.h>

#include "c011.h"
#include "common.h"

//#define DEBUG

int get_key(void)
{
    int c = getchar();
    return c;
}

void memdump (char *buf, int cnt) {
    char *p = buf;
    int byte_cnt=0;
    char str[200] = {'\0'};
    while (cnt > 0) {
        char b[10];
        sprintf (b, "0x%02X ", *p++);
        strcat (str,b);
        if (byte_cnt++==16) {
            printf ("%s\n", str);
            str[0] = '\0';
            byte_cnt=0;
        }
        cnt--;
    }
}

/* return true if loaded ok, false if error. */
bool load_buf (uint8_t *buf, int bcnt) {
    int wok,len;

    do {
        len = (bcnt > 255) ? 255 : bcnt;
        bcnt -= len;
        wok = tbyte_out(len);
        while (len-- && wok==0) {
            wok = tbyte_out(*buf++);
        } 
    } while (bcnt && wok==0);
    if (wok) {
        printf(" -- timeout loading network\n");
        return false;
    } else {
        printf ("Wrote bytes to network\n");
        return true;
    }
}

#include "FLBOOT.ARR"
#include "FLLOAD.ARR"
#include "IDENT.ARR"

bool boot(const char *fname)
{   int ack, fxp, nnodes;
    FILE *f = fopen (fname, "r");
    if (!f) {
        printf ("Couldn't open %s\n", fname);
        return false;
    }
    printf ("set byte mode...\n");
    c011_set_byte_mode();
    printf ("reset link...\n");
    rst_adpt();
    printf("Booting...\n");
    if (!load_buf(FLBOOT+22,sizeof(FLBOOT)-25)) {
        printf ("Failed to send FLBOOT\n");
        return false;
    }
    //daughter sends back an ACK word on the booted link
    if (word_in(&ack)) {
        printf(" -- timeout getting ACK\n");
        exit(1);
    }
    printf("ack = 0x%X\n", ack);
    printf("Loading...\n");      
    if (!load_buf(FLLOAD+22,sizeof(FLLOAD)-25)) {
        printf ("Failed to send FLLOAD\n");
        return false;
    }
    printf("ID'ing...\n");
    // IDENT will give master node ID 0 and other nodes ID 1
    if (!load_buf(IDENT+22,sizeof(IDENT)-25)) {
        printf ("Failed to send IDENT\n");
        return false;
    }
    if (tbyte_out(0))
    {
        printf(" -- timeout sending execute\n");
        return false;
    }
    if (tbyte_out(0))
    {
        printf(" -- timeout sending id\n");
        return false;
    }
    if (word_in(&nnodes)) {
        printf(" -- timeout getting nnodes (IDENT)\n");
        return false;
    }
    printf("from IDENT\n");
    printf("\tnodes found: %d (0x%X)\n",nnodes,nnodes);
    struct stat statbuf;
    stat (fname, &statbuf);
    uint8_t *code = (uint8_t *)malloc (statbuf.st_size);
    size_t rd = fread (code, statbuf.st_size, 1, f);
    if (rd != 1) {
        printf ("Failed to read %s\n", fname);
        return false;
    }
    printf("Sending %s (%d)\n", fname, statbuf.st_size);
    int r = load_buf(code+22, statbuf.st_size-25);
    if (tbyte_out(0))
    {
        printf("\n -- timeout sending execute");
        return false;
    }
    //mandel code sends these back to parent, so these will reach the host
    if (word_in(&nnodes)) {
        printf(" -- timeout getting nnodes (MANDEL)\n");
        return false;
    }
    if (word_in(&fxp)) {
        printf(" -- timeout getting fxp\n");
        return false;
    }
    printf("\nfrom MANDEL");
    printf("\n\tnodes found: %d (0x%X)",nnodes, nnodes);
    printf("\n\tFXP: %d (0x%X)\n",fxp, fxp);
    return true;
}

DEFINE_bool (verbose, false, "print verbose messages during initialisation");
DEFINE_string (code, "", "The code to load");

int main(int argc, char **argv) {
    int i,aok = 1;
    char *s;
    struct termios org_opts,new_opts;
    
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    int res = tcgetattr(STDIN_FILENO, &org_opts);
    memcpy (&new_opts, &org_opts, sizeof(new_opts));
    new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);
    
    init_lkio();
    boot(FLAGS_code.c_str());

    return(0);
}

