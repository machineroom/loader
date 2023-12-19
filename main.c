
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
#include <sstream>
#include <iomanip>


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

std::string hexstring (uint8_t *buf, unsigned int cnt) {
    std::ostringstream s;
    int byte_cnt=0;
    while (cnt > 0) {
        s << std::setw(2) << std::hex << *buf++;
        if (++byte_cnt==8) {
            s << "\n";
            byte_cnt=0;
        }
        cnt--;
    }
    return s.str();
}

/* return true if loaded ok, false if error. */
bool load_buf (uint8_t *buf, int bcnt, bool debug) {
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
        if (debug) printf ("Wrote bytes to network\n");
        return true;
    }
}

static int64_t read8s (uint8_t *buf) {
    int64_t tmp;
    tmp = buf[7];
    tmp <<= 8;
    tmp |= buf[6];
    tmp <<= 8;
    tmp |= buf[5];
    tmp <<= 8;
    tmp = buf[4];
    tmp <<= 8;
    tmp |= buf[3];
    tmp <<= 8;
    tmp |= buf[2];
    tmp <<= 8;
    tmp |= buf[1];
    tmp <<= 8;
    tmp |= buf[0];
    return tmp;
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

static uint32_t read4u (uint8_t *buf) {
    uint32_t tmp;
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

static int16_t read2s (uint8_t *buf) {
    int16_t tmp;
    tmp = buf[1];
    tmp <<= 8;
    tmp |= buf[0];
    return tmp;
}

void get_TLD_code(uint8_t *raw, std::vector<uint8_t> &code, bool debug) {
    uint8_t *raw_orig=raw;
    bool go = true;
    while (go) {
        if (debug) printf ("%4X %2d[0x%2X] ", (unsigned)(raw-raw_orig), raw[0], raw[0]);
        switch (raw[0]) {
            case 1:
                if (debug) printf ("REL_FILE: processor type = %d\n", raw[1]);
                raw += 1+1;
                break;
            case 2:
                if (debug) printf ("LIB_FILE: processor type = %d\n", raw[1]);
                raw += 1+1;
                break;
            case 3:
                if (debug) printf ("LD_FILE: processor type = %d\n", raw[1]);
                raw += 1+1;
                break;
            case 5:
                {
                    uint16_t line = read2u(&raw[1]);
                    if (debug) printf ("EOF line %u\n", line);
                    go = false;
                }
                break;
            case 10:
                {
                    uint16_t line = read2u(&raw[1]);
                    uint16_t size = read2u(&raw[3]);
                    if (debug) printf ("T_DATA %u %u\n", line, size);
                    code.insert (code.end(), &raw[5], &raw[5]+size);
                    raw += 1+2+2+size;
                }
                break;
            case 15:
                {
                    uint16_t line = read2u(&raw[1]);
                    int32_t size = read4s(&raw[3]);
                    if (debug) printf ("T_STORAGE %u %d\n", line, size);
                    code.insert (code.end(), size, 0u);
                    raw += 1+2+4;
                }
                break;
            case 22:
                {
                    int32_t address = read4s(&raw[1]);
                    if (debug) printf ("T_LOAD %X\n", address);
                    raw += 1+4;
                }
                break;
            case 23:
                {
                    int32_t address = read4s(&raw[1]);
                    if (debug) printf ("T_STACK %X\n", address);
                    raw += 1+4;
                }
                break;
            case 24:
                {
                    int32_t address = read4s(&raw[1]);
                    if (debug) printf ("T_ENTRY %X\n", address);
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
                    if (debug) printf ("T_DEBUG_DATA %u 0x%x %u %s\n", line, value, size, symbol);
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

bool load_tld (const char *name, bool debug) {
    std::vector<uint8_t> load;
    struct stat statbuf;
    stat (name, &statbuf);
    uint8_t *code = (uint8_t *)malloc (statbuf.st_size);
    FILE *f = fopen (name, "r");
    size_t rd = fread (code, statbuf.st_size, 1, f);
    if (rd != 1) {
        printf ("Failed to read %s\n", name);
        return false;
    }
    printf ("load (LSC) %s\n", name);
    get_TLD_code (code, load, debug);
    if (debug) {
        memdump(load.data(), load.size());
        printf("Sending %s (%ld)\n", name, load.size());
    }
    return load_buf(load.data(), load.size(), debug);
}

// TCOFF uses VL numbers (like transputer PFIX)
int get_number (uint8_t *raw, int64_t *value) {
    if (raw[0] <= 250) {
        *value = (int64_t)raw[0];
        return 1;
    } else if (raw[0] == 251) {
        //PFX1 (BYTE)
        *value = (int64_t)raw[0];
        return 1;
    } else if (raw[0] == 252) {
        //PFX2 (SHORT)
        *value = (int64_t)read2s(&raw[1]);
        return 1+2;
    } else if (raw[0] == 253) {
        //PFX3 (INT)
        *value = (int64_t)read4s(&raw[1]);
        return 1+4;
    } else if (raw[0] == 254) {
        //PFX4 (LONG)
        *value = (int64_t)read8s(&raw[1]);
        return 1+8;
    } else if (raw[0] == 255) {
        //SIGN
        return 1 + get_number(&raw[1], value);
    }
    return 0;
}

int get_string (uint8_t *raw, std::string &s) {
    int64_t length;
    int r = get_number(raw,&length);
    s.assign(&raw[1], &raw[1+length]);
    return 1+length;
}

int get_bool (uint8_t *raw, bool *b) {
    int64_t intb;
    int r = get_number(raw,&intb);
    assert(r==1);
    *b = (bool)intb;
    return r;
}

void get_TCOFF_code(uint8_t *raw, std::vector<uint8_t> &code, bool debug) {
    uint8_t *raw_orig=raw;
    bool go = true;
    while (go) {
        uint8_t tag;
        uint8_t length;
        tag = raw[0];
        length = raw[1];
        int r;
        if (debug) printf ("%4X T %2d[0x%2X] L %4d: ", (unsigned)(raw-raw_orig), tag, tag, length);
        raw += 2;   //skip TAG, length
        switch (tag) {
            case 2:
            {
                if (debug) printf ("START MODULE\n");
                int64_t sm_cpus;
                r = get_number(raw, &sm_cpus);
                length -= r;
                assert (length>=0);
                raw += r;
                printf ("\tsm_cpus = 0x%lx\n", sm_cpus);
                int64_t sm_attrib;
                r = get_number(raw, &sm_attrib);
                length -= r;
                assert (length>=0);
                raw += r;
                printf ("\tsm_attrib = 0x%lx\n", sm_attrib);
                int64_t sm_language;
                r = get_number(raw, &sm_language);
                length -= r;
                assert (length>=0);
                raw += r;
                printf ("\tsm_language = 0x%lx\n", sm_language);
                std::string sm_name;
                r = get_string(raw, sm_name);
                length -= r;
                // should have read everything, so length will be 0
                assert (length==0);
                raw += r;
                printf ("\tsm_name = %s\n", sm_name.c_str());
            }
            break;
            case 11:
            {
                if (debug) printf ("SECTION\n");
                int64_t se_section;
                r = get_number(raw, &se_section);
                length -= r;
                assert (length>=0);
                raw += r;
                printf ("\tse_section = 0x%lx\n", se_section);
                int64_t se_usage;
                r = get_number(raw, &se_usage);
                length -= r;
                assert (length>=0);
                raw += r;
                printf ("\tse_usage = 0x%lx\n", se_usage);
                std::string se_symbol;
                r = get_string(raw, se_symbol);
                length -= r;
                assert (length==0);
                raw += r;
                printf ("\tse_symbol = %s\n", se_symbol.c_str());

            }
            break;
            case 20:
            {
                if (debug) printf ("COMMENT\n");
                bool cm_copy;
                r = get_bool (raw, &cm_copy);
                length -= r;
                assert (length > 0);
                raw += r;
                printf ("\tcm_copy = %d\n", cm_copy);
                bool cm_print;
                r = get_bool (raw, &cm_print);
                length -= r;
                assert (length > 0);
                raw += r;
                printf ("\tcm_print = %d\n", cm_print);
                std::string cm_text;
                r = get_string (raw, cm_text);
                length -= r;
                assert (length == 0);
                raw += r;
                printf ("\tcm_text = %s\n", cm_text.c_str());
            }
            break;
            case 27:
            {
                if (debug) printf ("VERSION\n");
                std::string vn_tool_id;
                r = get_string(raw, vn_tool_id);
                length -= r;
                assert (length > 0);
                raw += r;
                printf ("\tvn_tool_id = %s\n", vn_tool_id.c_str());
                std::string vn_origin;
                r = get_string(raw, vn_origin);
                length -= r;
                assert (length >= 0);
                raw += r;
                printf ("\tvn_origin = %s\n", vn_origin.c_str());
                // should have read everything, so length will be 0
                assert (length==0);
            }
            break;
            case 28:
            {
                if (debug) printf ("LINKED_UNIT\n");
                raw+=length;
            }
            break;
            case 30:
            {
                if (debug) printf ("SYMBOL\n");
                int64_t sy_usage;
                r = get_number(raw, &sy_usage);
                length -= r;
                assert (length>=0);
                raw += r;
                printf ("\tsy_usage = 0x%lx\n", sy_usage);
                std::string sy_symbol;
                r = get_string(raw, sy_symbol);
                length -= r;
                assert (length == 0);
                raw += r;
                printf ("\tsy_symbol = %s\n", sy_symbol.c_str());

            }
            break;
            default:
                printf ("TAG %d *NOT HANDLED*\n", tag);
                go = false;
                break;
        }
    }
}

bool load_tcoff (const char *name, bool debug) {
    std::vector<uint8_t> load;
    struct stat statbuf;
    stat (name, &statbuf);
    uint8_t *code = (uint8_t *)malloc (statbuf.st_size);
    FILE *f = fopen (name, "r");
    size_t rd = fread (code, statbuf.st_size, 1, f);
    if (rd != 1) {
        printf ("Failed to read %s\n", name);
        return false;
    }
    printf ("load (TCOFF) %s\n", name);
    get_TCOFF_code (code, load, debug);
    if (debug) {
        memdump(load.data(), load.size());
        printf("Sending %s (%ld)\n", name, load.size());
    }
    return load_buf(load.data(), load.size(), debug);
}

bool boot(const char *fname, bool lsc, bool tcoff, bool debug)
{   
    #if 0
    int ack, nnodes;
    c011_init();
    printf ("set byte mode...\n");
    c011_set_byte_mode();
    printf ("reset link...\n");
    rst_adpt();
    printf("Booting...\n");
    // FLBOOT and friends are LSC TLD files
    if (!load_tld ("FLBOOT.TLD", debug))
    {
        exit(1);
    }
    //daughter sends back an ACK word on the booted link
    if (word_in(&ack)) {
        printf(" -- timeout getting ACK\n");
        exit(1);
    }
    printf("ack = 0x%X\n", ack);

    if (!load_tld ("FLLOAD.TLD", debug))
    {
        exit(1);
    }

    printf("ID'ing...\n");
    // IDENT will give master node ID 0 and other nodes ID 1
    if (!load_tld ("IDENT.TLD", debug))
    {
        exit(1);
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
    #endif
    if (lsc) {
        if (!load_tld (fname, debug))
        {
            exit(1);
        }
    } else if (tcoff) {
        if (!load_tcoff (fname, debug))
        {
            exit(1);
        }
    } else {
        fprintf (stderr, "TLD or TCOFF\n");
        exit(1);
    }
    return true;
}

#include <math.h>
DEFINE_string (code, "", "The code to load");
DEFINE_bool (lsc, false, "Load an LSC object");
DEFINE_bool (tcoff, false, "Load a TCOFF object");
DEFINE_bool (v, false, "verbose debug");
DEFINE_bool (m, false, "Mandelbrot");
DEFINE_bool (r, false, "Raytrace");

#define HIR  2.5
#define LOR  3.0e-14
#define f(x) sqrt(log(x)-log(LOR)+1.0)

#define ASPECT_R  0.75
#define MIN_SPAN  2.5
#define CENTER_R -0.75
#define CENTER_I  0.0
static int hicnt = 1024;
static int locnt = 150;

int calc_iter(double r) {
    if (r <= LOR) return(hicnt);
    return((int)((hicnt-locnt)/(f(LOR)-f(HIR))*(f(r)-f(HIR))+locnt+0.5));
}

void send_PRBCOM (double center_r, double center_i, double rng, int max_iter) {
    int len;
    double xrange,yrange;
    // This struct shared with transputer code (mandel.c) so type sizing & ordering is important
    struct{
        int32_t com;
        int32_t width;
        int32_t height;
        int32_t maxcnt;
        double lo_r;
        double lo_i;
        double gapx;
        double gapy;
    } prob_st;
    
    int FLAGS_width = 640;
    int FLAGS_height = 480;

    int esw,esh;
    double prop_fac,scale_fac;

    prop_fac = (ASPECT_R/((double)FLAGS_height/(double)FLAGS_width));
    esw = FLAGS_width;
    esh = FLAGS_height*prop_fac;

    if (esh <= esw) scale_fac = rng/(esh-1);
    else scale_fac = rng/(esw-1);

    xrange = scale_fac*(esw-1);
    yrange = scale_fac*(esh-1);
    prob_st.com = PRBCOM;
    prob_st.width = FLAGS_width;
    prob_st.height = FLAGS_height;
    prob_st.maxcnt = calc_iter(xrange);
    prob_st.lo_r = center_r - (xrange/2.0);
    prob_st.lo_i = center_i - (yrange/2.0);
    prob_st.gapx = xrange / (FLAGS_width-1);
    prob_st.gapy = yrange / (FLAGS_height-1);
#ifdef DEBUG
    printf ("PRBCOM struct:\n\tcom:%d\n\twidth:%d\n\theight:%d\n\tmaxcnt:%d\n\tlo_r:%lf\n\tlo_i:%lf\n\tgapx:%lf\n\tgapy:%lf\n",
             prob_st.com, prob_st.width, prob_st.height, prob_st.maxcnt, prob_st.lo_r, prob_st.lo_i, prob_st.gapx, prob_st.gapy);
    memdump ((uint8_t *)&prob_st,sizeof(prob_st));
#endif
    assert (sizeof(prob_st) == 48);
    if (word_out(sizeof(prob_st)) != 0) {
        printf(" -- timeout sending prob_st size\n");
        exit(1);           
    }
    if (chan_out((char *)&prob_st,sizeof(prob_st)) != 0) {
        printf(" -- timeout sending prob_st\n");
        exit(1);           
    }
#ifdef DEBUG
    printf ("PRBCOM sent\n");
#endif    
}

#define DEBUG
void do_mandel(void) {
    int fxp, nnodes;
    // BOOT
    if (tbyte_out(0))
    {
        printf("\n -- timeout sending execute");
        exit(1);
    }
    //mandel code sends these back to parent, so these will reach the host
    if (word_in(&nnodes)) {
        printf(" -- timeout getting nnodes (MANDEL)\n");
        exit(1);
    }
    if (word_in(&fxp)) {
        printf(" -- timeout getting fxp\n");
        exit(1);
    }
    printf("\nfrom MANDEL");
    printf("\n\tnodes found: %d (0x%X)",nnodes, nnodes);
    printf("\n\tFXP: %d (0x%X)\n",fxp, fxp);
    //RENDER
    double center_r = CENTER_R;
    double center_i = CENTER_I;
    double rng = 3.335073;
    int max_iter = 145;

	while (1)
	{
        int len;
        send_PRBCOM (center_r, center_i, rng, max_iter);
        // wait for finish
        while (1) {
            if (word_in(&len) == 0) {      //len in bytes
                int32_t buf[RSLCOM_BUFSIZE];
                if (chan_in ((char *)buf,len) == 0) {
#ifdef DEBUG
                    printf ("len=0x%X, buf = 0x%X 0x%X 0x%X 0x%X...0x%X\n",len,buf[0],buf[1],buf[2],buf[3],buf[(len/4)-1]);
#endif
                    if (buf[0] == FLHCOM) {
                        printf ("Finished\n");
                        break;
                    } 
                }
            }
        }
        // zoom
        rng -= 0.5;
	}

}

void do_raytrace(void) {
    while (1) {
        int x;
        if (word_in(&x) == 0) {      //len in bytes
#ifdef DEBUG
            printf ("from RT 0x%X\n", x);
#endif
        }
    }
}

int main(int argc, char **argv) {
    int i,aok = 1;
    char *s;
    struct termios org_opts,new_opts;
    
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    /*
    int res = tcgetattr(STDIN_FILENO, &org_opts);
    memcpy (&new_opts, &org_opts, sizeof(new_opts));
    new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);
    */

    boot(FLAGS_code.c_str(), FLAGS_lsc, FLAGS_tcoff, FLAGS_v);
    if (FLAGS_m) {
        do_mandel();
    } else if (FLAGS_r) {
        do_raytrace();
    } else {
        fprintf (stderr, "Not doing any host code!\n");
    }

    return(0);
}

