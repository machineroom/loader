/********************************* MAN.C ************************************
*  (C) Copyright 1987-1993  Computer System Architects, Provo UT.           *
*  This Mandelbrot program is the property of Computer System Architects    *
*  (CSA) and is provided only as an example of a transputer/PC program for  *
*  use  with CSA's Transputer Education Kit and other transputer products.  *
*  You may freely distribute copies or modifiy the program as a whole or in *
*  part, provided you insert in each copy appropriate copyright notices and *
*  disclaimer of warranty and send to CSA a copy of any modifications which *
*  you plan to distribute.                                                  *
*  This program is provided as is without warranty of any kind. CSA is not  *
*  responsible for any damages arising out of the use of this program.      *
****************************************************************************/
/****************************************************************************
* This program man.c is the main part of mandelbrot. It includes setting up *
* the screen and user options. It calls appropiate driver functions *
* to reset and send data to and from network of transputers.                *
****************************************************************************/


#include <stdio.h>
#include <math.h>
#include "LKIO.H"
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include "c011.h"
#include "fb.h"

#define JOBCOM 0L
#define PRBCOM 1L
#define DATCOM 2L
#define RSLCOM 3L
#define FLHCOM 4L

#define TRUE 1
#define FALSE 0

#define LARW 0x14b
#define RARW 0x14d
#define UARW 0x148
#define DARW 0x150
#define PGUP 0x149
#define PGDN 0x151
#define HOME 0x147
#define END  0x14f
#define INS  0x152
#define DEL  0x153
#define ESC  0x1b
#define ENTR 0x0d
#define NONE 0
#define INSM    0x200

#define BUFSIZE 20
#define REAL    double
#define loop    for (;;)

#define AUTOFILE "MAN.DAT"
#define PAUSE    1

#define HIR  2.5
#define LOR  3.0e-14
#define f(x) sqrt(log(x)-log(LOR)+1.0)

//#define DEBUG

typedef unsigned int uint;

void com_loop(void);
void auto_loop(void);
void scan_tran(void);
void scan_host(void);
int iterate(REAL,REAL,int);
int calc_iter(double);
void init_window(void);
void save_coord(void);
void zoom(int *);
void region(int *, int *, int *, int *, int *);
void draw_box(int,int,int,int);
void boot_mandel(void);
int load_buf(char *, int);

static bool autz;
static bool verbose = false;
static bool host = false;
static int hicnt = 1024;
static int locnt = 150;
static int mxcnt;
static int ps = PAUSE;
static int screen_w;
static int screen_h;
static void (*scan)(void) = scan_host;
static FILE *fpauto;
static bool immediate_render = false;
static uint8_t *screen_buffer = NULL;
static bool use_sdl_render = true;

SDL_Renderer *sdl_renderer;
uint32_t *fbptr = NULL;
int fb_width;
int fb_height;

int get_key(void)
{
    SDL_Event event;
    SDL_KeyboardEvent key;
    int ret = SDL_WaitEvent(&event);
    if (ret==1) {
        if (event.type == SDL_KEYUP) {
            key = event.key;
            switch (key.keysym.scancode) {
                case SDL_SCANCODE_PAGEUP:
                    return PGUP;
                case SDL_SCANCODE_PAGEDOWN:
                    return PGDN;
                case SDL_SCANCODE_HOME:
                    return HOME;
                case SDL_SCANCODE_END:
                    return END;
                case SDL_SCANCODE_DOWN:
                    return UARW;
                case SDL_SCANCODE_UP:
                    return DARW;
                case SDL_SCANCODE_LEFT:
                    return LARW;
                case SDL_SCANCODE_RIGHT:
                    return RARW;
                case SDL_SCANCODE_INSERT:
                    return INS;
                case SDL_SCANCODE_DELETE:
                    return DEL;
                case SDL_SCANCODE_ESCAPE:
                    return ESC;
                case SDL_SCANCODE_RETURN:
                    return ENTR;
            }
        } else if (event.type == SDL_WINDOWEVENT) {
            switch (event.window.event) {
                case SDL_WINDOWEVENT_CLOSE:
                    return END;
            }
        }
    }
    return NONE;
}

int main(int argc, char **argv) {
    int i,aok = 1;
    char *s;

	screen_w = 640; screen_h = 480;

    printf("CSA Mandelzoom Version 2.1 for PC\n");
    printf("(C) Copyright 1988 Computer System Architects Provo, Utah\n");
    printf("Enhanced by Axel Muhr (geekdot.com), 2009, 2015\n");
    printf("Enhanced by James Wilson (macihenroomfiddling@gmail.com) 2019, 2020\n");
    printf("This is a free software and you are welcome to redistribute it\n\n");

    for (i = 1; i < argc && argv[i][0] == '-'; i++)
    {
        for (s = argv[i]+1; *s != '\0'; s++)
        {
            switch (*s)
            {
                case 'a':
                    if (sscanf(s+1,"%d",&ps) == 1) s++;
                    if ((ps < 0) || (ps > 9)) ps = PAUSE;
                    autz = true;
                    break;
                case 'w':
                    if (i >= argc) {aok = 0; break;}
                    aok &= sscanf(argv[++i],"%i",&screen_w) == 1 && *(s+1) == '\0';
                    break;
                case 'h':
                    if (i >= argc) {aok = 0; break;}
                    aok &= sscanf(argv[++i],"%i",&screen_h) == 1 && *(s+1) == '\0';
                    break;
                case 'i':
                    if (i >= argc) {aok = 0; break;}
                    aok &= sscanf(argv[++i],"%i",&mxcnt) == 1 && *(s+1) == '\0';
                    break;
                case 't':
                    host = true;
                    break;
                case 'f':
                    use_sdl_render = false;
                    break;
                case 'x':
                    verbose = true;
                    break;
                case 'r':
                    immediate_render = true;
                    break;   
                default:
                    aok = 0;
                    break;
            }
        }
    }
    aok &= i == argc;
    if (!aok) {
        printf("Usage: man [-b #] [-i #] [-a[#]cd#ehpt]\n");
        printf("  -a  auto-zoom, coord. from file 'man.dat', # sec. pause\n");
        printf("  -i  max. iteration count, # is iter. (default variable)\n");
        printf("  -t  use host, no transputers (default transputers)\n");
        printf("  -x  print verbose messages during initialisation\n");
        printf("  -r  render each vector as received (slower)\n");
        printf("  -w  width\n");
        printf("  -h  height\n");
        printf("  -f  direct FB access (default SDL2)\n");
        exit(1);
    }

    if (use_sdl_render) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            return 1;
        }
    } else {
        if (SDL_Init(SDL_INIT_TIMER) < 0) {
            return 1;
        }
    }
    if (!host) {
        init_lkio(0,0,0);
        boot_mandel();
        scan = scan_tran;
    }
    c011_dump_stats("done boot");
	if (autz) {
	    if ((fpauto = fopen(AUTOFILE,"r")) == NULL) {
            printf(" -- can't open file: %s\n",AUTOFILE);
            exit(1);
	    }
	} else {
        printf("\nAfter frame is displayed:\n\n");
        printf("Home - display zoom box, use arrow keys to move & size, ");
        printf("Home again to zoom\n");
        printf("Ins  - toggle between size and move zoom box\n");
        printf("PgUp - reset to outermost frame\n");
        printf("PgDn - save coord. and iter. to file 'man.dat'\n");
        printf("End  - quit\n");
    }
    
    if (!immediate_render) {
        screen_buffer = malloc(screen_w * screen_h);
    }
    if (use_sdl_render) {
        SDL_Window *window = SDL_CreateWindow("T-Mandel with SDL",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w,
                screen_h, SDL_WINDOW_SHOWN);

        if (window == NULL) {
            SDL_Quit();
            return 2;
        }
        sdl_renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED) ;
    } else {
        fbptr = FB_Init(&fb_width, &fb_height);
        if (fbptr == NULL) {
            return 1;
        }
    }
    init_window();

    if (autz)
        auto_loop();
    else
        com_loop();

    return(0);
}

typedef struct {
    unsigned char b;
    unsigned char g;
    unsigned char r;
} BGR;

BGR ega_palette[16] = {
    {0,0,0},
    {0,0,127},
    {0,0,255},      //2=red
    {0,63,255},
    {0,127,255},
    {0,255,255},    //5=yellow
    {0,127,63},
    {0,255,0},      //7=green
    {127,255,0},
    {255,255,0},    //9=cyan
    {255,127,0},
    {255,63,0},
    {255,0,0},      //12=blue
    {255,0,63},
    {255,63,255},   //14=magenta
    {255,255,255}
};

uint32_t ega_palette_ARGB[16] = {
    0x00000000,
    0x007F0000,
    0x00FF0000,     //2=red
    0x00FF3F00,
    0x00FF7F00,
    0x00FFFF00,     //5=yellow
    0x003F7F00,
    0x0000FF00,     //7=green
    0x0000FF7F,
    0x0000FFFF,     //9=cyan
    0x00007FFF,
    0x00003FFF,
    0x000000FF,     //12=blue
    0x003F00FF,
    0x00FF3FFF,     //14=magenta
    0x00FFFFFF
};

//x,y - start point
//buf_size - number of pixels in buf
//buf - array of pixel colour indices (range 0-15)
void vect (int x, int y, int buf_size, unsigned char *buf) {
    if (immediate_render) {
        if (use_sdl_render) {
            //immediate render looks cool, but is ultimately slower due to SDL overhead
            for (int i=0; i < buf_size; i++) {
                SDL_SetRenderDrawColor(sdl_renderer,
                                       ega_palette[buf[i]].r,
                                       ega_palette[buf[i]].g,
                                       ega_palette[buf[i]].b,
                                       0xFF);
                SDL_RenderDrawPoint(sdl_renderer, x+i, y);
            }
            SDL_RenderPresent(sdl_renderer);
        } else {
            uint32_t *bp = &fbptr[(y*fb_width)+(x)];
            for (int i=0; i < buf_size; i++) {
                *bp++ = ega_palette_ARGB[buf[i]];
            }
        }
    } else {
        //delayed render just copies calculated slice to screen buffer
        memcpy (&screen_buffer[(y*screen_w)+x], buf, buf_size);
    }
}

void render_screen(void) {
    int i=0;
    if (use_sdl_render) {
        for (int y=0; y < screen_h; y++) {
            for (int x=0; x < screen_w; x++) {
                SDL_SetRenderDrawColor(sdl_renderer,
                                       ega_palette[screen_buffer[i]].r, 
                                       ega_palette[screen_buffer[i]].g,
                                       ega_palette[screen_buffer[i]].b,
                                       0xFF);
                SDL_RenderDrawPoint(sdl_renderer, x, y);
                i++;
             }
        }
        SDL_RenderPresent(sdl_renderer);
    } else {
        uint32_t *bp = fbptr;
        for (int y=0; y < screen_h; y++) {
            for (int x=0; x < screen_w; x++) {
                *bp++ = ega_palette_ARGB[screen_buffer[i++]];
            }
            bp += (fb_width - screen_w);
        }
    }
}

#define ASPECT_R  0.75
#define MIN_SPAN  2.5
#define CENTER_R -0.75
#define CENTER_I  0.0

int esw,esh;
double center_r,center_i;
double prop_fac,scale_fac;

void com_loop(void)
{
    int esc;
    bool render = true;
    loop
    {
        if (render) {
            Uint64 start, compute, render;
            start = SDL_GetPerformanceCounter();
            (*scan)();
            compute = SDL_GetPerformanceCounter();
            printf ("scan took %f ms\n", (double)((compute - start)*1000) / SDL_GetPerformanceFrequency()); 
            c011_dump_stats("done scan");
            if (!immediate_render) {
                render_screen();
                render = SDL_GetPerformanceCounter();
                printf ("render took %f ms\n", (double)((render - compute)*1000) / SDL_GetPerformanceFrequency()); 
            }
        }
        switch (get_key())
        {
            case HOME:
                zoom(&esc);
                render = true;
                break;
            case PGUP:
                init_window(); 
                render = true;
                break;
            case PGDN:
                save_coord();
                break;
            case END :
                return;
            default:
                render=false;
                break;
        }
    }
}

void auto_loop(void) {
    int res;
    double rng;
    int run = 1;
    
    loop
	{
        res = fscanf(fpauto," x:%lf y:%lf range:%lf iter:%d", &center_r,&center_i,&rng,&mxcnt);
        printf ("start scan %lf %lf %lf %d\n", center_r, center_i, rng, mxcnt);
        if (res == EOF) {
            rewind(fpauto);
            continue;
        }
        if (res != 4) return;   /* End if anything else returned */
        scale_fac = rng/(esw-1);
        Uint64 start, now;
        start = SDL_GetPerformanceCounter();
        (*scan)();              /* this does the work */
        now = SDL_GetPerformanceCounter();
        printf ("scan took %f ms\n", (double)((now - start)*1000) / SDL_GetPerformanceFrequency()); 
        c011_dump_stats("done scan");
        run++;
        sleep(ps);
    }
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

void scan_tran(void) {
    register int len;
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
    
    int32_t buf[BUFSIZE];

    xrange = scale_fac*(esw-1);
    yrange = scale_fac*(esh-1);
    prob_st.com = PRBCOM;
    prob_st.width = screen_w;
    prob_st.height = screen_h;
    prob_st.maxcnt = calc_iter(xrange);
    prob_st.lo_r = center_r - (xrange/2.0);
    prob_st.lo_i = center_i - (yrange/2.0);
    prob_st.gapx = xrange / (screen_w-1);
    prob_st.gapy = yrange / (screen_h-1);
#ifdef DEBUG
    printf ("PRBCOM struct:\n\tcom:%ld\n\twidth:%ld\n\theight:%ld\n\tmaxcnt:%ld\n\tlo_r:%lf\n\tlo_i:%lf\n\tgapx:%lf\n\tgapy:%lf\n",
             prob_st.com, prob_st.width, prob_st.height, prob_st.maxcnt, prob_st.lo_r, prob_st.lo_i, prob_st.gapx, prob_st.gapy);
    memdump ((char *)&prob_st,sizeof(prob_st));
#endif
    assert (sizeof(prob_st) == 48);
    word_out(sizeof(prob_st));
    chan_out((char *)&prob_st,sizeof(prob_st));
#ifdef DEBUG
    printf ("PRBCOM sent\n");
#endif

	loop
	{
	    len = (int)word_in();       //len in bytes
	    assert (len < sizeof(buf));
		chan_in ((char *)buf,len);
#ifdef DEBUG
		printf ("len=0x%X, buf = 0x%X 0x%X 0x%X 0x%X...0x%X\n",len,buf[0],buf[1],buf[2],buf[3],buf[(len/4)-1]);
#endif
		if (buf[0] == FLHCOM) {
		    break;
		} else if (buf[0] == RSLCOM) {
		    //buf=[RSLCOM,x,y,pixels]
		    //76-(3*4)=64
            vect((int)buf[1],(int)buf[2],len-3*4,(unsigned char *)&buf[3]);
		}
	}
}

void scan_host(void) {
    int x,y,maxcnt;
    double xrange,yrange;
    double lo_r;
    double lo_i;
    double gapx;
    double gapy;
    REAL cx,cy;
    unsigned char buf[screen_w];

    xrange = scale_fac*(esw-1);
    yrange = scale_fac*(esh-1);
    maxcnt = calc_iter(xrange);
    lo_r = center_r - (xrange/2.0);
    lo_i = center_i - (yrange/2.0);
    gapx = xrange / (screen_w-1);
    gapy = yrange / (screen_h-1);

    for (y = 0; y < screen_h; y++) {
        cy = y*gapy+lo_i;
        for (x = 0; x < screen_w; x++) {
            cx = (x)*gapx+lo_r;
            buf[x] = iterate(cx,cy,maxcnt);
        }
        vect (0,y,sizeof(buf),buf);
	}
}

int iterate(REAL cx, REAL cy, int maxcnt) {
	int cnt;
	REAL zx,zy,zx2,zy2,tmp,four;
	
	four = 4.0;
	zx = zy = zx2 = zy2 = 0.0;
	cnt = 0;
	do {
	    tmp = zx2-zy2+cx;
	    zy = zx*zy*2.0+cy;
	    zx = tmp;
	    zx2 = zx*zx;
	    zy2 = zy*zy;
	    cnt++;
	} while (cnt < maxcnt && zx2+zy2 < four);
	if (cnt != maxcnt) {
	    return(cnt);
	}
	return(0);
}

int calc_iter(double r) {
    if (mxcnt) return(mxcnt);
    if (r <= LOR) return(hicnt);
    return((int)((hicnt-locnt)/(f(LOR)-f(HIR))*(f(r)-f(HIR))+locnt+0.5));
}

void init_window(void) {
    prop_fac = (ASPECT_R/((double)screen_h/(double)screen_w));
    esw = screen_w;
    esh = screen_h*prop_fac;
    if (esh <= esw) scale_fac = MIN_SPAN/(esh-1);
    else scale_fac = MIN_SPAN/(esw-1);
    center_r = CENTER_R;
    center_i = CENTER_I;
}

void save_coord(void) {
    double xrange;

    if (!autz) {
        if ((fpauto = fopen(AUTOFILE,"a")) == NULL) return;
        autz = true;
    }
    xrange = scale_fac*(esw-1);
    fprintf(fpauto,"x:%17.14f y:%17.14f range:%e iter:%d\n", center_r,center_i,xrange,calc_iter(xrange));
}

void zoom(int *esc) {
    int bx,by,lx,ly,sw,sh;
    double xfac,yfac;

    region(&bx,&by,&lx,&ly,esc);
    if (*esc) return;
    by = by*prop_fac;
    ly = ly*prop_fac;
    sw = ((bx < lx) ? lx-bx : bx-lx) + 1;
    sh = ((by < ly) ? ly-by : by-ly) + 1;
    center_r += (((bx+lx) - (esw-1))/2.0)*scale_fac;
    center_i += (((by+ly) - (esh-1))/2.0)*scale_fac;
    xfac = (double)(sw-1)/(double)(esw-1);
    yfac = (double)(sh-1)/(double)(esh-1);
    scale_fac *= (xfac > yfac) ? xfac : yfac;
}

void region(int *bx, int *by, int *lx, int *ly, int *esc) {
    int c,tmp,dx,dy,jmp,boxw,boxh,insert;

    insert = 0;
    jmp = 10;
    boxw = screen_w/10;
    boxh = screen_h/10;
    *esc = FALSE;
    *bx = ((screen_w - boxw)/2);
    *by = ((screen_h - boxh)/2);
    *lx = *bx + (boxw-1);
    *ly = *by + (boxh-1);
    draw_box(*bx,*by,*lx,*ly);
    do {
        c = get_key()+insert;
        draw_box(*bx,*by,*lx,*ly);
        switch (c) {
            case LARW:
                tmp = (*bx >= jmp) ? jmp : *bx;
                *bx -= tmp; *lx -= tmp;
                break;
            case RARW:
                tmp = (*lx+jmp < screen_w) ? jmp : (screen_w-1) - *lx;
                *bx += tmp; *lx += tmp;
                break;
            case UARW:
                tmp = (*ly+jmp < screen_h) ? jmp : (screen_h-1) - *ly;
                *by += tmp; *ly += tmp;
                break;
            case DARW:
                tmp = (*by >= jmp) ? jmp : *by;
                *by -= tmp; *ly -= tmp;
                break;
            case LARW+INSM:
            case DARW+INSM:
                tmp = *lx-*bx;
                dx = (tmp > jmp) ? tmp-jmp : 1;
                dy = (double)screen_h / (double)screen_w * (double)dx;
                *bx += (tmp>>1)-(dx>>1); *lx = *bx+dx;
                *by += ((*ly-*by)>>1)-(dy>>1); *ly = *by+dy;
                break;
            case RARW+INSM:
            case UARW+INSM:
                tmp = *lx-*bx;
                dx = (tmp+jmp < screen_w) ? tmp+jmp : screen_w-1;
                dy = (double)screen_h / (double)screen_w * (double)dx;
                *bx += (tmp>>1)-(dx>>1); *lx = *bx+dx;
                *by += ((*ly-*by)>>1)-(dy>>1); *ly = *by+dy;
                if (*bx < 0) {
                    *lx -= *bx; *bx = 0;
                } else if (*lx >= screen_w) {
                    *bx -= *lx-(screen_w-1); *lx = screen_w-1;
                }
                if (*by < 0) {
                    *ly -= *by; *by = 0;
                } else if (*ly >= screen_h) {
                    *by -= *ly-(screen_h-1); *ly = screen_h-1;
                }
                break;
            case INS:
            case INS+INSM:
                insert ^= INSM;
                break;
            case ESC:
            case ESC+INSM:
                *esc = TRUE;
                break;
        }
        draw_box(*bx,*by,*lx,*ly);
    } while ((c & (INSM-1)) != HOME && !*esc);
    draw_box(*bx,*by,*lx,*ly);
}

void draw_box(int x1, int y1, int x2, int y2) {
    int x,y,w,h;
    if (x1 < x2) {x = x1; w = x2-x1+1;}
    else {x = x2; w = x1-x2+1;}
    if (y1 < y2) {y = y1; h = y2-y1+1;}
    else {y = y2; h = y1-y2+1;}
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_SetRenderDrawColor(sdl_renderer, 0xFF, 0XFF, 0xFF, 0xFF);
    SDL_RenderDrawRect(sdl_renderer, &rect);
    SDL_RenderPresent(sdl_renderer);
}

#include "FLBOOT.ARR"
#include "FLLOAD.ARR"
#include "IDENT.ARR"
#include "MANDEL.ARR"
#ifdef SMALL
    #include "SMALLMAN.ARR"
#endif

void boot_mandel(void)
{   int ack, fxp, only_2k, nnodes;

    rst_adpt(TRUE);
    if (verbose) printf("Booting...\n");
    if (!load_buf(FLBOOT,sizeof(FLBOOT))) exit(1);
    //daughter sends back an ACK word on the booted link
    ack = (int)word_in();
    printf("ack = 0x%X\n", ack);
    if (verbose) printf("Loading...\n");      
    if (!load_buf(FLLOAD,sizeof(FLLOAD))) exit(1);
    if (verbose) printf("ID'ing...\n");
    if (!load_buf(IDENT,sizeof(IDENT))) exit(1);
    if (!tbyte_out(0))
    {
	    printf(" -- timeout sending execute\n");
        exit(1);
    }
    if (!tbyte_out(0))
    {
        printf(" -- timeout sending id\n");
        exit(1);
    }
    only_2k = (int)word_in();
    nnodes  = (int)word_in();
    fxp     = (int)word_in();
    printf("\nfrom IDENT");
    printf("\n\tnodes found: %d",nnodes);
    printf("\n\tnodes with only 2K RAM: %d",only_2k);
    printf("\n\tFXP: %d",fxp);
    printf("\n\tusing %s-point arith.\n", fxp ? "fixed" : "floating");
#ifdef SMALL
    if (only_2k)
    {
        if (verbose) printf("Sending 2k mandel-code\n");
        if (!load_buf(SMALLMAN,sizeof(SMALLMAN))) exit(1);
        //send 0 (marker for no more code). FLLOAD will start executing the loaded code
        if (!tbyte_out(0))
        {
            printf(" -- timeout sending execute\n");
            exit(1);
        }
#else
    if (only_2k) {
        printf ("You need to enable 2K support\n");
        exit(-1);
#endif
    } else {
        if (verbose) printf("Sending mandel-code\n");	   
        if (!load_buf(MANDEL,sizeof(MANDEL))) exit(1);
        if (!tbyte_out(0))
        {
            printf(" -- timeout sending execute\n");
            exit(1);
        }
        //mandel code sends these back to parent, so these will reach the host
        nnodes = word_in();
        fxp = word_in();
        printf("\nfrom MANDEL");
        printf("\n\tnodes found: %d",nnodes);
        printf("\n\tFXP: %d\n",fxp);
    }
    //mandel operates in word mode from now on. Clear BYTE mode on HSL cards for full throughput
    c011_clear_byte_mode();
}

/* return TRUE if loaded ok, FALSE if error. */
int load_buf (char *buf, int bcnt) {
    int wok,len;

    do {
        len = (bcnt > 255) ? 255 : bcnt;
        bcnt -= len;
        if ((wok = tbyte_out(len)))
            while (len-- && (wok = tbyte_out(*buf++)));
        } while (bcnt && wok);
    if (!wok) printf(" -- timeout loading network\n");
    return(wok);
}


