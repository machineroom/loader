#include "load_mandel.h"
#include "rcommon.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "lkio.h"

#define HIR  2.5
#define LOR  3.0e-14
#define f(x) sqrt(log(x)-log(LOR)+1.0)

#define ASPECT_R  0.75
#define MIN_SPAN  2.5
#define CENTER_R -0.75
#define CENTER_I  0.0
static int hicnt = 1024;
static int locnt = 150;

static int calc_iter(double r) {
    if (r <= LOR) return(hicnt);
    return((int)((hicnt-locnt)/(f(LOR)-f(HIR))*(f(r)-f(HIR))+locnt+0.5));
}

static void send_PRBCOM (double center_r, double center_i, double rng, int max_iter) {
    int len;
    double xrange,yrange;
    // This struct shared with transputer code (raytrace.c) so type sizing & ordering is important
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
void do_raytrace(void) {
    int fxp, nnodes;
    // BOOT
    if (tbyte_out(0))
    {
        printf("\n -- timeout sending execute");
        exit(1);
    }
    //raytrace code sends these back to parent, so these will reach the host
    if (word_in(&nnodes)) {
        printf(" -- timeout getting nnodes (RAYTRACE)\n");
        exit(1);
    }
    if (word_in(&fxp)) {
        printf(" -- timeout getting fxp\n");
        exit(1);
    }
    printf("\nfrom RAYTRACE");
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
        //rng -= 0.5;
	}

}