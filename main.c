
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

#include <vector>


//#define DEBUG

int get_key(void)
{
    int c = getchar();
    return c;
}

void memdump (uint8_t *buf, unsigned int cnt) {
    uint8_t *p = buf;
    int byte_cnt=0;
    char str[8192] = {'\0'};
    while (cnt > 0) {
        char b[10];
        sprintf (b, "%02X ", *p++);
        strcat (str,b);
        if (++byte_cnt==8) {
            printf ("%s\n", str);
            str[0] = '\0';
            byte_cnt=0;
        }
        cnt--;
    }
    printf ("%s\n", str);
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

static int32_t read4s (uint8_t *buf) {
    int32_t tmp;
    tmp = buf[3];
    tmp <<= 8;
    tmp |= buf[2];
    tmp <<= 8;
    tmp |= buf[1];
    tmp <<= 8;
    tmp |= buf[0];
    return tmp;
}

static uint16_t read2u (uint8_t *buf) {
    uint16_t tmp;
    tmp = buf[1];
    tmp <<= 8;
    tmp |= buf[0];
    return tmp;
}

void get_code(uint8_t *raw, std::vector<uint8_t> &code) {
    uint8_t *raw_orig=raw;
    bool go = true;
    while (go) {
        printf ("%4X %2d[0x%2X] ", (unsigned)(raw-raw_orig), raw[0], raw[0]);
        switch (raw[0]) {
            case 0:
                printf ("RESERVED *NOT HANDLED*\n");
                go = false;
                break;
            case 1:
                printf ("REL_FILE: processor type = %d\n", raw[1]);
                raw += 1+1;
                break;
            case 2:
                printf ("LIB_FILE: processor type = %d\n", raw[1]);
                raw += 1+1;
                break;
            case 3:
                printf ("LD_FILE: processor type = %d\n", raw[1]);
                raw += 1+1;
                break;
            case 4:
                printf ("SIZE *NOT HANDLED*\n");
                go = false;
                break;
            case 5:
                {
                    uint16_t line = read2u(&raw[1]);
                    printf ("EOF line %u\n", line);
                    go = false;
                }
                break;
            case 10:
                {
                    uint16_t line = read2u(&raw[1]);
                    uint16_t size = read2u(&raw[3]);
                    printf ("T_DATA %u %u\n", line, size);
                    code.insert (code.end(), &raw[5], &raw[5]+size);
                    raw += 1+2+2+size;
                }
                break;
            case 15:
                {
                    uint16_t line = read2u(&raw[1]);
                    int32_t size = read4s(&raw[3]);
                    printf ("T_STORAGE %u %d\n", line, size);
                    raw += 1+2+4;
                }
                break;
            case 22:
                {
                    int32_t address = read4s(&raw[1]);
                    printf ("T_LOAD %X\n", address);
                    raw += 1+4;
                }
                break;
            case 23:
                {
                    int32_t address = read4s(&raw[1]);
                    printf ("T_STACK %X\n", address);
                    raw += 1+4;
                }
                break;
            case 24:
                {
                    int32_t address = read4s(&raw[1]);
                    printf ("T_ENTRY %X\n", address);
                    raw += 1+4;
                }
                break;
            case 25:
                {
                    uint16_t line = read2u(&raw[1]);
                    int32_t value = read4s(&raw[3]);
                    uint16_t size = read2u(&raw[7]);
                    uint8_t symbol_length = raw[9];
                    char symbol[256]={0};
                    memcpy (symbol, &raw[10], size);
                    printf ("T_DEBUG_DATA %u 0x%x %u %s\n", line, value, size, symbol);
                    raw += 1+2+4+2+size;
                }
                break;
            default:
                printf ("%d *NOT HANDLED*\n", raw[0]);
                go = false;
                break;

        }
    }
}

bool boot(const char *fname, bool lsc)
{   
    int ack, fxp, nnodes;
    std::vector<uint8_t> load;
    FILE *f = fopen (fname, "r");
    if (!f) {
        printf ("Couldn't open %s\n", fname);
        return false;
    }/*
    c011_init();
    printf ("set byte mode...\n");
    c011_set_byte_mode();
    printf ("reset link...\n");
    rst_adpt();
    printf("Booting...\n");
    // FLBOOT and friends are LSCs
    get_code (FLBOOT, load);
    if (!load_buf(load.data(),load.size())) {
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
    load.clear();
    get_code (FLLOAD, load);
    if (!load_buf(load.data(),load.size())) {
        printf ("Failed to send FLLOAD\n");
        return false;
    }

    printf("ID'ing...\n");
    // IDENT will give master node ID 0 and other nodes ID 1
    load.clear();
    get_code (IDENT, load);
    if (!load_buf(load.data(),load.size())) {
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
    */
    struct stat statbuf;
    stat (fname, &statbuf);
    uint8_t *code = (uint8_t *)malloc (statbuf.st_size);
    size_t rd = fread (code, statbuf.st_size, 1, f);
    if (rd != 1) {
        printf ("Failed to read %s\n", fname);
        return false;
    }
    printf ("load %s\n", fname);
    get_code (code, load);
    memdump(load.data(), load.size());
    /*
    printf("Sending %s (%ld)\n", fname, statbuf.st_size);
    int r = load_buf(load.data(), load.size());
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
    */
    return true;
}

DEFINE_bool (verbose, false, "print verbose messages during initialisation");
DEFINE_string (code, "", "The code to load");
DEFINE_bool (lsc, false, "LSC object format parsing");

int main(int argc, char **argv) {
    int i,aok = 1;
    char *s;
    struct termios org_opts,new_opts;
    
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    int res = tcgetattr(STDIN_FILENO, &org_opts);
    memcpy (&new_opts, &org_opts, sizeof(new_opts));
    new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);
    
    //init_lkio();
    boot(FLAGS_code.c_str(), FLAGS_lsc);

    return(0);
}

