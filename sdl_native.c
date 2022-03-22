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
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <gflags/gflags.h>
#include "common.h"

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

#define REAL    double
#define loop    for (;;)

#define AUTOFILE "MAN.DAT"

#define HIR  2.5
#define LOR  3.0e-14
#define f(x) sqrt(log(x)-log(LOR)+1.0)

//#define DEBUG

typedef unsigned int uint;

void com_loop(void);
void auto_loop(void);
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
void init_pal256();

static bool host = false;
static int hicnt = 1024;
static int locnt = 150;
static void (*scan)(void) = scan_host;
static FILE *fpauto;
static uint8_t *screen_buffer = NULL;

SDL_Renderer *sdl_renderer;
uint32_t *fbptr = NULL;
int fb_width;
int fb_height;
int fb_bpp;

DEFINE_int32 (width, 640, "width");
DEFINE_int32 (height, 480, "height");
DEFINE_int32 (max_iter, 0, "max. iteration count, # is iter. (default variable)");
DEFINE_bool (verbose, false, "print verbose messages during initialisation");
DEFINE_bool (immediate, false, "render each vector as received (slower)");
DEFINE_bool (sdl, true, "Use SDL2 render, otherwise direct FB");
DEFINE_bool (host, false, "use host, no transputers (default transputers)");
DEFINE_bool (auto, false, "auto-zoom, coord. from file 'man.dat'");
DEFINE_bool (keepgoing, false, "with auto will repeat forever");
DEFINE_int32 (pause, 1, "# sec. pause (for auto mode)");

int main(int argc, char **argv) {
    int i,aok = 1;
    char *s;
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    init_pal256();

    printf("CSA Mandelzoom Version 2.1 for PC\n");
    printf("(C) Copyright 1988 Computer System Architects Provo, Utah\n");
    printf("Enhanced by Axel Muhr (geekdot.com), 2009, 2015\n");
    printf("Enhanced by James Wilson (macihenroomfiddling@gmail.com) 2019, 2020\n");
    printf("This is a free software and you are welcome to redistribute it\n\n");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return 1;
    }
    
    if (!FLAGS_immediate) {
        screen_buffer = (uint8_t*)malloc(FLAGS_width * FLAGS_height);
    }
    SDL_Window *window = SDL_CreateWindow("T-Mandel with SDL",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, FLAGS_width,
            FLAGS_height, SDL_WINDOW_SHOWN);

    if (window == NULL) {
        SDL_Quit();
        return 2;
    }
    sdl_renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED) ;

    init_window();

	if (FLAGS_auto) {
	    if ((fpauto = fopen(AUTOFILE,"r")) == NULL) {
            printf(" -- can't open file: %s\n",AUTOFILE);
            exit(1);
	    }
        auto_loop();
	} else {
        printf("\nAfter frame is displayed:\n\n");
        printf("Home - display zoom box, use arrow keys to move & size, ");
        printf("Home again to zoom\n");
        printf("Ins  - toggle between size and move zoom box\n");
        printf("PgUp - reset to outermost frame\n");
        printf("PgDn - save coord. and iter. to file 'man.dat'\n");
        printf("End  - quit\n");
        com_loop();
    }
    
    return(0);
}

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

uint32_t ega_palette_ARGB32[16] = {
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
//RRRR RGGG GGGB BBBB
uint16_t ega_palette_ARGB16[16] = {
    0x0000,
    0x7800,     //1=mid red
    0xF800,     //2=red
    0xF9E0,
    0xFBE0,
    0xFFE0,     //5=yellow
    0x3BE0,
    0x07E0,     //7=green
    0x07EF,
    0x07FF,     //9=cyan
    0x03FF,
    0x01FF,
    0x001F,     //12=blue
    0x381F,
    0xF91F,     //14=magenta
    0xFFFF      //15=white
};

//x,y - start point
//buf_size - number of pixels in buf
//buf - array of pixel colour indices (range 0-15)
void vect (int x, int y, int buf_size, unsigned char *buf) {
    //delayed render just copies calculated slice to screen buffer
    memcpy (&screen_buffer[(y*FLAGS_width)+x], buf, buf_size);
}

BGR PAL256[256];

// a somewhat satisfuying, but very ripped off palette
void init_pal256(void) {
    for (int i = 0; i < 256; i++)
    {
        PAL256[i].r = 13*(256-i) % 256;
        PAL256[i].g = 7*(256-i) % 256;
        PAL256[i].b = 11*(256-i) % 256;
    }
}

void render_screen(void) {
    int i=0;
    for (int y=0; y < FLAGS_height; y++) {
        for (int x=0; x < FLAGS_width; x++) {
            if (screen_buffer[i] == 0) {
                //point inside set (dark grey)
                SDL_SetRenderDrawColor(sdl_renderer,
                                       20,
                                       20,
                                       20,
                                       0xFF);
            } else {
                SDL_SetRenderDrawColor(sdl_renderer,
                                       PAL256[screen_buffer[i]].r, 
                                       PAL256[screen_buffer[i]].g,
                                       PAL256[screen_buffer[i]].b,
                                       0xFF);
            }
            SDL_RenderDrawPoint(sdl_renderer, x, y);
            i++;
         }
    }
    SDL_RenderPresent(sdl_renderer);
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
            printf ("scan took %0.1f ms\n", (double)((compute - start)*1000) / SDL_GetPerformanceFrequency()); 
            if (!FLAGS_immediate) {
                render_screen();
                render = SDL_GetPerformanceCounter();
                printf ("render took %0.1f ms\n", (double)((render - compute)*1000) / SDL_GetPerformanceFrequency()); 
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
    double min=9999999;
    double max=0;
    double time;
    
    loop
	{
        res = fscanf(fpauto," x:%lf y:%lf range:%lf iter:%d", &center_r,&center_i,&rng,&FLAGS_max_iter);
        if (res == EOF) {
            rewind(fpauto);
            continue;
        }
        if (res != 4) {
            /* Hit a non-valid line*/
            if (FLAGS_keepgoing) {
                rewind(fpauto);
                continue;
            } else {
                return;
            }
        }
        printf ("start scan %lf %lf %lf %d\n", center_r, center_i, rng, FLAGS_max_iter);
        scale_fac = rng/(esw-1);
        Uint64 start, now;
        start = SDL_GetPerformanceCounter();
        (*scan)();              /* this does the work */
        now = SDL_GetPerformanceCounter();

        time = (double)((now - start)*1000) / SDL_GetPerformanceFrequency();
        if (time < min) min = time;
        if (time > max) max = time;
        printf ("scan took %0.1f ms ([%0.1f %0.1f]\n", time,min,max); 
        run++;
        sleep(FLAGS_pause);
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

void scan_host(void) {
    int x,y,maxcnt;
    double xrange,yrange;
    double lo_r;
    double lo_i;
    double gapx;
    double gapy;
    REAL cx,cy;
    unsigned char buf[FLAGS_width];
    int highest=0;

    xrange = scale_fac*(esw-1);
    yrange = scale_fac*(esh-1);
    maxcnt = calc_iter(xrange);
    lo_r = center_r - (xrange/2.0);
    lo_i = center_i - (yrange/2.0);
    gapx = xrange / (FLAGS_width-1);
    gapy = yrange / (FLAGS_height-1);

    for (y = 0; y < FLAGS_height; y++) {
        cy = y*gapy+lo_i;
        for (x = 0; x < FLAGS_width; x++) {
            cx = (x)*gapx+lo_r;
            buf[x] = iterate(cx,cy,maxcnt);
            if (buf[x] > highest) {
                highest = buf[x];
            }
        }
        vect (0,y,sizeof(buf),buf);
	}
    printf ("highest count is %d\n",highest);
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
    if (FLAGS_max_iter) return(FLAGS_max_iter);
    if (r <= LOR) return(hicnt);
    return((int)((hicnt-locnt)/(f(LOR)-f(HIR))*(f(r)-f(HIR))+locnt+0.5));
}

void init_window(void) {
    prop_fac = (ASPECT_R/((double)FLAGS_height/(double)FLAGS_width));
    esw = FLAGS_width;
    esh = FLAGS_height*prop_fac;
    if (esh <= esw) scale_fac = MIN_SPAN/(esh-1);
    else scale_fac = MIN_SPAN/(esw-1);
    center_r = CENTER_R;
    center_i = CENTER_I;
}

void save_coord(void) {
    double xrange;

    if (!FLAGS_auto) {
        if ((fpauto = fopen(AUTOFILE,"a")) == NULL) return;
        FLAGS_auto = true;
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
    boxw = FLAGS_width/10;
    boxh = FLAGS_height/10;
    *esc = FALSE;
    *bx = ((FLAGS_width - boxw)/2);
    *by = ((FLAGS_height - boxh)/2);
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
                tmp = (*lx+jmp < FLAGS_width) ? jmp : (FLAGS_width-1) - *lx;
                *bx += tmp; *lx += tmp;
                break;
            case UARW:
                tmp = (*ly+jmp < FLAGS_height) ? jmp : (FLAGS_height-1) - *ly;
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
                dy = (double)FLAGS_height / (double)FLAGS_width * (double)dx;
                *bx += (tmp>>1)-(dx>>1); *lx = *bx+dx;
                *by += ((*ly-*by)>>1)-(dy>>1); *ly = *by+dy;
                break;
            case RARW+INSM:
            case UARW+INSM:
                tmp = *lx-*bx;
                dx = (tmp+jmp < FLAGS_width) ? tmp+jmp : FLAGS_width-1;
                dy = (double)FLAGS_height / (double)FLAGS_width * (double)dx;
                *bx += (tmp>>1)-(dx>>1); *lx = *bx+dx;
                *by += ((*ly-*by)>>1)-(dy>>1); *ly = *by+dy;
                if (*bx < 0) {
                    *lx -= *bx; *bx = 0;
                } else if (*lx >= FLAGS_width) {
                    *bx -= *lx-(FLAGS_width-1); *lx = FLAGS_width-1;
                }
                if (*by < 0) {
                    *ly -= *by; *by = 0;
                } else if (*ly >= FLAGS_height) {
                    *by -= *ly-(FLAGS_height-1); *ly = FLAGS_height-1;
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



