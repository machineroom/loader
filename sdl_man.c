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
#include <unistd.h>
#include <SDL2/SDL.h>

//legacy B004 link stuff
#define LINK_BASE 0x150
#define DMA_CHAN  1

#define JOBCOM 0L
#define PRBCOM 1L
#define DATCOM 2L
#define RSLCOM 3L
#define FLHCOM 4L

#define TRUE 1
#define FALSE 0

#define F1   0x13b
#define F2   0x13c
#define F3   0x13d
#define F4   0x13e
#define F5   0x13f
#define F6   0x140
#define F7   0x141
#define F8   0x142
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

#define INSM    0x200
#define MAXPIX  64
#define BUFSIZE 20
#define REAL    double
#define loop    for (;;)

#define AUTOFILE "man.dat"
#define PAUSE    3

#define HIR  2.5
#define LOR  3.0e-14
#define f(x) sqrt(log(x)-log(LOR)+1.0)

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
int get_key(void);
void waitsec(int);
void draw_box(int,int,int,int);
void boot_mandel(void);
int load_buf(char *, int);
int getch(void);
int kbhit(void);

static int autz;
static int verbose;
static int host;
static int hicnt = 1024;
static int locnt = 150;
static int mxcnt;
static int lb = LINK_BASE;
static int dc = 0;
static int ps = PAUSE;
static int screen_w;
static int screen_h;
static void (*scan)(void) = scan_host;
static void (*data_in)(char *, uint) = chan_in;
static FILE *fpauto;

static unsigned lt;
static int D[2][2] = {
    {0,2},
    {3,1}
};

int getch(void) {
}

int kbhit(void) {
    SDL_Event test_event;
}

SDL_Renderer *sdl_renderer;

int main(int argc, char **argv) {
    int i,aok = 1;
    char *s;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return 1;
    }
    
   
    printf("CSA Mandelzoom Version 2.1 for PC\n");
    printf("(C) Copyright 1988 Computer System Architects Provo, Utah\n");
    printf("Enhanced by Axel Muhr (geekdot.com), 2009, 2015\n");
    printf("Enhanced by James Wilson (macihenroomfiddling@gmail.com) 2019\n");
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
                    autz = 1;
                    break;
                case 'b':
                    if (i >= argc) {aok = 0; break;}
                    aok &= sscanf(argv[++i],"%i",&lb) == 1 && *(s+1) == '\0';
                    break;
                case 'd':
                    aok &= sscanf(s+1,"%d",&dc) == 1;
                    if (aok) s++;
                    aok &= (dc >= 1) && (dc <= 3);
                    break;
                case 'i':
                    if (i >= argc) {aok = 0; break;}
                    aok &= sscanf(argv[++i],"%i",&mxcnt) == 1 && *(s+1) == '\0';
                    break;
                case 'p':
                    data_in = dma_in;
                    dc = DMA_CHAN;
                    break;
                case 't':
                    host = 1;
                    break;
                case 'x':
                    verbose = 1;
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
        printf("  -b  link adaptor base address, # is base (default 0x150)\n");
        printf("  -d  DMA channel, # can be 1-3 (default 1)\n");
        printf("  -i  max. iteration count, # is iter. (default variable)\n");
        printf("  -p  use program I/O, no DMA (default use DMA)\n");
        printf("  -t  use host, no transputers (default transputers)\n");
        printf("  -x  print verbose messages during initialisation\n");
        exit(1);
    }
    if (!host) {
        init_lkio(lb,dc,dc);
        boot_mandel();
        scan = scan_tran;
    }
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
        printf("\n -- Press a key to continue --\n"); 
        getch();
    }
	screen_w = 640; screen_h = 480;

    SDL_Window *window = SDL_CreateWindow("T-Mandel with SDL",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w,
            screen_h, SDL_WINDOW_SHOWN);

    if (window == NULL) {
        SDL_Quit();
        return 2;
    }
    sdl_renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) ;

    init_window();

    if (dc) dma_on();
    if (autz) auto_loop(); else com_loop();
    if (dc) dma_off();

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
    {255,0,0},        //12=blue
    {255,0,63},
    {255,63,255},        //14=magenta
    {255,255,255}
};

void vect (int x, int y, int pixvec, unsigned char *buf) {
    // Now we can draw our points
    for (int i=0; i < pixvec; i++) {
        SDL_SetRenderDrawColor(sdl_renderer, ega_palette[buf[i]].r, ega_palette[buf[i]].g, ega_palette[buf[i]].b, 0xFF);
        SDL_RenderDrawPoint(sdl_renderer, x+i, y);
    }
    // Set the color to what was before
    SDL_SetRenderDrawColor(sdl_renderer, 0x00, 0x00, 0x00, 0xFF);
    // .. you could do some other drawing here
    // And now we present everything we draw after the clear.
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
    esc = FALSE;
    loop
    {
        if (!esc) (*scan)();
        while (kbhit()) getch();
        switch (get_key())
        {
            case HOME: zoom(&esc); break;
            case PGUP: init_window(); esc = FALSE; break;
            case PGDN: save_coord(); esc = TRUE; break;
            case END : return;
            default: esc = TRUE;
        }
        sleep(1);
    }
}

void auto_loop(void) {
    int res;
    double rng;
    int run = 1;
    
    loop
	{
        res = fscanf(fpauto," x:%lf y:%lf range:%lf iter:%d", &center_r,&center_i,&rng,&mxcnt);
        if (res == EOF) {
            rewind(fpauto);
            continue;
        }
        if (res != 4) return;   /* End if anything else returned */
        scale_fac = rng/(esw-1);
        (*scan)();              /* this does the work */
        run++;
        if (kbhit()) break;
        sleep(ps);
        if (kbhit()) break;
    }
    do {
        getch(); 
    } while (kbhit());
}



void scan_tran(void) {
    register int len;
    double xrange,yrange;
	struct{
        long com;
        long width;
        long height;
        long maxcnt;
        double lo_r;
        double lo_i;
        double gapx;
        double gapy;
    } prob_st;
    
	long buf[BUFSIZE];

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
    word_out(12L*4);
    chan_out((char *)&prob_st,12*4);

	loop
	{
		//(*data_in)((char *)buf,len = (int)word_in());
		if (len == 4) break;
		//(*vect)((int)buf[1],(int)buf[2],len-3*4,(char *)&buf[3]);
	}
}

void scan_host(void) {
    int i,pixvec;
    int x,y,maxcnt,multiple;
    double xrange,yrange;
    double lo_r;
    double lo_i;
    double gapx;
    double gapy;
    REAL cx,cy;
    unsigned char buf[MAXPIX];

    xrange = scale_fac*(esw-1);
    yrange = scale_fac*(esh-1);
    maxcnt = calc_iter(xrange);
    lo_r = center_r - (xrange/2.0);
    lo_i = center_i - (yrange/2.0);
    gapx = xrange / (screen_w-1);
    gapy = yrange / (screen_h-1);

    multiple = screen_w/MAXPIX*MAXPIX;
    for (y = 0; y < screen_h; y++) {
        cy = y*gapy+lo_i;
        for (x = 0; x < screen_w; x+=MAXPIX) {
            pixvec = (x < multiple) ? MAXPIX : screen_w-multiple;
            for (i = 0; i < pixvec; i++) {
                cx = (x+i)*gapx+lo_r;
                buf[i] = iterate(cx,cy,maxcnt);
            }
            vect (x,y,pixvec,buf);
            //(*vect)(x,y,pixvec,buf);
            if (kbhit()) return;
        }
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
        autz = TRUE;
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
    jmp = 1;
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

int get_key(void)
{
}

void draw_box(int x1, int y1, int x2, int y2) {
    int x,y,w,h;
    if (x1 < x2) {x = x1; w = x2-x1+1;}
    else {x = x2; w = x1-x2+1;}
    if (y1 < y2) {y = y1; h = y2-y1+1;}
    else {y = y2; h = y1-y2+1;}
}

#define MAXROWS 350
#define MAXCOLS 640
#define COLBYTES MAXCOLS/8
#define GRAFOUT(index,value) {outp(0x3Ce,index); outp(0x3Cf,value);}

#include "SRESET.ARR"
#include "FLBOOT.ARR"
#include "FLLOAD.ARR"
#include "IDENT.ARR"
#include "MANDEL.ARR"
#include "SMALLMAN.ARR"

void boot_mandel(void)
{   int fxp, only_2k, nnodes;

    rst_adpt(TRUE);
    if (verbose) printf("Resetting Transputers...");
    if (!load_buf(SRESET,sizeof(SRESET))) exit(1);
    if (verbose) printf("Booting...");
    if (!load_buf(FLBOOT,sizeof(FLBOOT))) exit(1);
    if (verbose) printf("Loading...");      
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
    if (verbose) printf("Done. Getting data for \"only_2k\",");        
    only_2k = (int)word_in();
    if (verbose) printf("\"num_nodes\",");
    nnodes  = (int)word_in();
    if (verbose) printf("\"fpx\",");
    fxp     = (int)word_in();
    printf("\nnodes found: %d",nnodes);
    if (only_2k) printf("\nnodes have only 2K RAM");
    printf("\nusing %s-point arith.\n", fxp ? "fixed" : "floating");
    if (only_2k)
    {
        if (verbose) printf("Sending 2k mandel-code\n");
        if (!load_buf(SMALLMAN,sizeof(SMALLMAN))) exit(1);
        if (!tbyte_out(0))
        {
            printf(" -- timeout sending execute\n");
            exit(1);
        }
    } else {
        if (verbose) printf("Sending mandel-code\n");	   
        if (!load_buf(MANDEL,sizeof(MANDEL))) exit(1);
        if (!tbyte_out(0))
        {
            printf(" -- timeout sending execute\n");
            exit(1);
        }
        word_in();    /*throw away nnodes*/
        word_in();    /*throw away fixed point*/
    }
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

