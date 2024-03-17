
#include <string.h>
#include <stdio.h>
#include "lkio.h"
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <gflags/gflags.h>
#include <termios.h>
#include <sys/stat.h>

#include "c011.h"

#include <vector>
#include <sstream>
#include <iomanip>

#include "load_mandel.h"

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
        //memdump(load.data(), load.size());
        printf("Sending %s (%ld)\n", name, load.size());
    }
    return load_buf(load.data(), load.size(), debug);
}

// TCOFF uses VL numbers (like transputer PFIX)
void get_number (uint8_t **raw, int64_t *length, int64_t *value) {
    if (**raw <= 250) {
        assert (*length >= 1);
        *value = (int64_t)**raw;
        *length -= 1;
        *raw += 1;
    } else if (**raw == 251) {
        //PFX1 (BYTE)
        assert (*length >= 1);
        *value = (int64_t)**raw;
        *length -= 1;
        *raw += 1;
    } else if (**raw == 252) {
        //PFX2 (SHORT)
        assert (*length >= 3);
        *value = (int64_t)read2s((*raw)+1);
        *length -= 3;
        *raw += 3;
    } else if (**raw == 253) {
        //PFX3 (INT)
        assert (*length >= 5);
        *value = (int64_t)read4s((*raw)+1);
        *length -= 5;
        *raw += 5;
    } else if (**raw == 254) {
        //PFX4 (LONG)
        assert (*length >= 9);
        *value = (int64_t)read8s((*raw)+1);
        *length -= 9;
        *raw += 9;
    } else if (**raw == 255) {
        //SIGN
        assert (false);//TODO
    } else {
        assert (false);
    }
}

void get_string (uint8_t **raw, int64_t *length, std::string &s) {
    int64_t l;
    get_number(raw,length,&l);
    s.assign((*raw), (*raw)+l);
    *length -= l;
    *raw += l;
}

void get_bool (uint8_t **raw, int64_t *length, bool *b) {
    int64_t intb;
    get_number(raw,length,&intb);
    *b = (bool)intb;
}

void get_value (uint8_t **raw, int64_t *length, int64_t *value) {
    int64_t value_tag;
    get_number(raw, length, &value_tag);
    switch (value_tag) {
        case 1: // CO_VALUE_TAG
            get_number (raw, length, value);
            break;
        default:
            printf ("unhandled value TAG %ld\n", value_tag);
            assert(false);
            break;
    }
}

void get_TCOFF_code(uint8_t *raw, off64_t raw_size, std::vector<uint8_t> &code, bool debug) {
    uint8_t *raw_orig=raw;
    bool go = true;
    while (go) {
        int64_t tag;
        int64_t length;
        int64_t l;
        if (raw>=raw_orig+raw_size) {
            printf ("read all tags\n");
            go = false;
            break;
        }
        get_number(&raw,&l,&tag);
        get_number(&raw,&l,&length);
        if (debug) printf ("%4X T %2d[0x%2X] L %4ld: ", (unsigned)(raw-raw_orig), (uint8_t)tag, (uint8_t)tag, length);
        switch (tag) {
            case 2:
            {
                if (debug) printf ("START MODULE\n");
                int64_t sm_cpus;
                get_number(&raw, &length, &sm_cpus);
                printf ("\tsm_cpus = 0x%lx\n", sm_cpus);
                int64_t sm_attrib;
                get_number(&raw, &length, &sm_attrib);
                printf ("\tsm_attrib = 0x%lx\n", sm_attrib);
                int64_t sm_language;
                get_number(&raw, &length, &sm_language);
                printf ("\tsm_language = 0x%lx\n", sm_language);
                std::string sm_name;
                get_string(&raw, &length, sm_name);
                printf ("\tsm_name = %s\n", sm_name.c_str());
            }
            break;
            case 3:
            {
                if (debug) printf ("END_MODULE\n");
            }
            break;
            case 4:
            {
                if (debug) printf ("SET_LOAD_POINT\n");
                int64_t sl_location;
                get_number(&raw, &length, &sl_location);
                printf ("\tsl_location = %ld\n", sl_location);
            }
            break;
            case 5:
            {
                if (debug) printf ("ADJUST_POINT\n");
                int64_t aj_offset;
                get_value(&raw, &length, &aj_offset);
                printf ("\taj_offset = %ld\n", aj_offset);
            }
            break;
            case 6:
            {
                if (debug) printf ("LOAD_TEXT\n");
                std::string lt_text;
                get_string(&raw, &length, lt_text);
                printf ("\tlt_text = <code>(%ld bytes)\n", lt_text.size());
                code.insert(code.end(), lt_text.data(), lt_text.data() + lt_text.size());
            }
            break;
            case 11:
            {
                if (debug) printf ("SECTION\n");
                int64_t se_section;
                get_number(&raw, &length, &se_section);
                printf ("\tse_section = %ld\n", se_section);
                int64_t se_usage;
                get_number(&raw, &length, &se_usage);
                printf ("\tse_usage = 0x%lx\n", se_usage);
                std::string se_symbol;
                get_string(&raw, &length, se_symbol);
                printf ("\tse_symbol = %s\n", se_symbol.c_str());
            }
            break;
            case 12:
            {
                if (debug) printf ("DEFINE_MAIN\n");
                int64_t dm_entry;
                get_number(&raw, &length, &dm_entry);
                printf ("\tdm_entry = %ld\n", dm_entry);
            }
            break;
            case 15:
            {
                if (debug) printf ("DEFINE_SYMBOL\n");
                int64_t ds_ident;
                get_number(&raw, &length, &ds_ident);
                printf ("\tds_ident = %ld\n", ds_ident);
                int64_t ds_value;
                get_value (&raw, &length, &ds_value);
                printf ("\tds_value = %ld\n", ds_value);
            }
            break;
            case 20:
            {
                if (debug) printf ("COMMENT\n");
                bool cm_copy;
                get_bool (&raw, &length, &cm_copy);
                printf ("\tcm_copy = %d\n", cm_copy);
                bool cm_print;
                get_bool (&raw, &length, &cm_print);
                printf ("\tcm_print = %d\n", cm_print);
                std::string cm_text;
                get_string (&raw, &length, cm_text);
                printf ("\tcm_text = %s\n", cm_text.c_str());
            }
            break;
            case 26:
            {
                if (debug) printf ("DESCRIPTOR\n");
                int64_t de_symbol;
                get_number (&raw, &length, &de_symbol);
                printf ("\tde_symbol = %ld\n", de_symbol);
                int64_t de_language;
                get_number (&raw, &length, &de_language);
                printf ("\tde_language = %ld\n", de_language);
                std::string de_string;
                get_string (&raw, &length, de_string);
                printf ("\tde_string = %s\n", de_string.c_str());
            }
            break;
            case 27:
            {
                if (debug) printf ("VERSION\n");
                std::string vn_tool_id;
                get_string(&raw, &length, vn_tool_id);
                printf ("\tvn_tool_id = %s\n", vn_tool_id.c_str());
                std::string vn_origin;
                get_string(&raw, &length, vn_origin);
                printf ("\tvn_origin = %s\n", vn_origin.c_str());
            }
            break;
            case 28:
            {
                if (debug) printf ("LINKED_UNIT\n");
            }
            break;
            case 30:
            {
                if (debug) printf ("SYMBOL\n");
                int64_t sy_usage;
                get_number(&raw, &length, &sy_usage);
                printf ("\tsy_usage = 0x%lx\n", sy_usage);
                std::string sy_symbol;
                get_string(&raw, &length, sy_symbol);
                printf ("\tsy_symbol = %s\n", sy_symbol.c_str());

            }
            break;
            default:
                printf ("TAG %ld *NOT HANDLED*\n", tag);
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
    get_TCOFF_code (code, statbuf.st_size, load, debug);
    if (debug) {
        //memdump(load.data(), load.size());
        printf("Sending %s (%ld)\n", name, load.size());
    }
    return load_buf(load.data(), load.size(), debug);
}

bool boot(const char *fname, bool lsc, bool tcoff, bool debug)
{   
    #if 1
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

DEFINE_string (code, "", "The code to load");
DEFINE_bool (lsc, false, "Load an LSC object");
DEFINE_bool (tcoff, false, "Load a TCOFF object");
DEFINE_bool (v, false, "verbose debug");
DEFINE_bool (m, false, "Mandelbrot");
DEFINE_bool (r, false, "Raytrace");



void do_raytrace(void) {
    printf ("Sit and wait for network\n");
    while (1) {
        int x;
        if (word_in(&x) == 0) {      //len in bytes
            printf ("from RT 0x%X\n", x);
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

