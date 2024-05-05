/******************************** RAYTRACE.C ********************************
 * Inspired by...
*  (C) Copyright 1987-1993  Computer System Architects, Provo UT.           *
*  This  program is the property of Computer System Architects (CSA)        *
*  and is provided only as an example of a transputer/PC program for        *
*  use  with CSA's Transputer Education Kit and other transputer products.  *
*  You may freely distribute copies or modifiy the program as a whole or in *
*  part, provided you insert in each copy appropriate copyright notices and *
*  disclaimer of warranty and send to CSA a copy of any modifications which *
*  you plan to distribute.						    *
*  This program is provided as is without warranty of any kind. CSA is not  *
*  responsible for any damages arising out of the use of this program. 
* And.. oc-ray from Inmos
****************************************************************************/


#include <conc.h>
#include <math.h>
#include <stdlib.h>
#include <stddef.h>
#include <conc.h>
#include "mlibp.h"
#include "loader.h"

#include "rcommon.h"
#include "B438.h"

/*  define constants*/
#define TRUE  1
#define FALSE 0

#define THRESH 2.0e-7
#define loop for (;;)

#define   T805  0x0a
#define   T805L 0x9
#define   T805H	0x14
#define   T800D 0x60
#define   T414B 0x61
#pragma define LIGHTS

#pragma asm
    .T800
    .NOEMULATE
#pragma endasm

/* lights on whitecross uses error LED on each processor */
#pragma macro lon()
#pragma ifdef LIGHTS
#pragma asm
    seterr
#pragma endasm
#pragma endif
#pragma endmacro

#pragma macro loff()
#pragma ifdef LIGHTS
#pragma asm
    testerr
#pragma endasm
#pragma endif
#pragma endmacro

#define JOBWSZ 2048+MAXPIX
#define FEDWSZ 2048
#define SELWSZ 2048
#define BUFWSZ 2048
#define ARBWSZ 2048+MAXPIX

int jobws[JOBWSZ];
int bufws[3][BUFWSZ];
int selws[SELWSZ];
int feedws[FEDWSZ];
int arbws[ARBWSZ];

/* NOTE main() must be first function */

main(LOADGB *ld)
{
    int i,fxp;
    if (ld->id == 0) {
        setupGfx(1);
    }

    /* get num nodes and whether floating point*/
    {
        int nodes;
        int buf[2];

        nodes = fxp = 0;
        /* LSC89 compiler bug? ld->dn_out[1] & ld->dn_out[2] are both set but loop skips these (at least on the root node)
           LSC93 seems to generate very similar code. jumps out of loop if ld->dn_out[0]==0 so link 1&2 are ignored! 
           Suspect that with older FLBOOT code [0] was incorrectly set so this problem was masked */
        for (i = 0; i < 3; i++)
        {
            if (ld->dn_out[i]!=(void *)0) {
                ChanIn(ld->dn_out[i]+4,(char *)buf,2*4);
                nodes += buf[0]; fxp |= buf[1];
            }
        }
        fxp |= !((ld->trantype == T800D) ||
            ((ld->trantype  > T805L) && (ld->trantype < T805H)) );
        buf[0] = nodes+1;
        buf[1] = fxp;
        ChanOut(ld->up_in-4,(char *)buf,2*4);
    }
    /* set up par structure*/
    /* host -> feed -> selector -> job
                                -> buffer (HARD) -> feed
                                -> buffer (HARD) -> feed
                                -> buffer (HARD) -> feed
    host <- arbiter <- job
                    <- (HARD) arbiter <- job
                    <- (HARD) arbiter <- job
                    <- (HARD) arbiter <- job 
    */
    {
        Channel *si,*so[5],*sr[5],*ai[5];
        extern void job(),buffer(),feed(),arbiter(),selector();
        int list_index;

        /* job worker on each node - this does iter() */
        sr[0] = ChanAlloc();    /* sr = selector request (job request channel) */
        so[0] = ChanAlloc();    /* so = selector outputs (job configuration channel) */
        ai[0] = ChanAlloc();    /* ai = arbiter inputs (job/buffer results written to these channels) */
        /* job(Channel *req_out, Channel *job_in, Channel *rsl_out) */
        PRun(PSetup(jobws,job,JOBWSZ,3,sr[0],so[0],ai[0]));
        /* buffer worker for each child node */
        /* The channel lists must be zero terminated. ld->dn_out may have 0, 1 or 2 links set and in any order,
           i.e [0,0,linkA], [linkB,0,linkA] & [linkA,linkB,linkC] are all valid */
        list_index=1;
        for (i = 0; i < 3; i++)
        {
            if (ld->dn_out[i]!=(void *)0) {
                sr[list_index] = ChanAlloc();
                so[list_index] = ChanAlloc();
                ai[list_index] = ld->dn_out[i]+4;   /* input link from child */
                /* buffer (Channel *req_out, Channel *buf_in, Channel *buf_out) */
                PRun(PSetup(bufws[i],buffer,BUFWSZ,3,sr[list_index],so[list_index],ld->dn_out[i]));
                list_index++;
            } 
        }
        sr[list_index] = 0;
        so[list_index] = 0;
        ai[list_index] = 0;
        /* Allocate si (selector input) chanel. For root node this is wired to the output of the feed process.
           Normal worker nodes have their si wired to the input side of the parent link (ld->up_in) */
        /* ld->id is set in IDENT to 0 by host and OR'd with 1 for every other node */
        if (ld->id)
        {
            /* id!=0 == worker node */
            si = ld->up_in;
            /* arbiter(Channel **arb_in, Channel *arb_out, int root) */
            PRun(PSetup(arbws,arbiter,ARBWSZ,3,ai,ld->up_in-4,0));
        }
        else
        {
            /* id=0 == root node */
            si = ChanAlloc();
            /* feed(Channel *host_in, Channel *host_out, Channel *fd_out, int fxp) */
            PRun(PSetup(feedws,feed,FEDWSZ,4,ld->up_in,ld->up_in-4,si,fxp));
            /* arbiter(Channel **arb_in, Channel *arb_out, int root) */
            PRun(PSetup(arbws,arbiter,ARBWSZ,3,ai,ld->up_in-4,1));
        }
        /* selector(Channel *sel_in, Channel **req_in, Channel **dn_out) */
        PRun(PSetup(selws,selector,SELWSZ,3,si,sr,so));
        PStop();
    }
}

/* arb_in is list of channels connected to job or buffer processes */
/* arb_out is the output side of the parent link */
/* Receives result from worker (local job or link buffer processes) and passes result to parent */
void arbiter(Channel **arb_in, Channel *arb_out, int root)
{
    int i,len,cnt,pri;
    int buf[MAXPIX];
    int type;

    cnt = pri = 0;
    loop
    {
        /* 1. Wait for worker to notify result */
        i = ProcPriAltList(arb_in,pri);
        pri = (arb_in[pri+1]) ? pri+1 : 0;
        type = ChanInInt(arb_in[i]);
        if (type == c_patch) {
            patch p;
            ChanIn(arb_in[i],(char *)&p,sizeof(p));
            ChanIn(arb_in[i],buf,p.patchWidth*p.patchHeight*4);
            if (root) {
                /* render to B438 */
                int *a = (int *)0x80400000;
                int x,y;
                i = 0;
                a += (p.y*640)+p.x;
                for (y=0; y < p.patchHeight; y++) {
                    for (x=0; x < p.patchWidth; x++) {
                        a[x] = buf[i++];
                    }
                    a += 640;
                }
            } else {
                /* 3. Send result to parent */
                ChanOutInt(arb_out,type);
                ChanOut(arb_out,(char *)&p,sizeof(p));
                ChanOut(arb_out,(char *)buf,p.patchWidth*p.patchHeight*4);
            }
        } else {
            while(1){lon();}
        }
    }
}

void normalize ( float *vector, float oldHyp ) {
    float t;
    t =  (vector [0] * vector [0]) +
        ((vector [1] * vector [1]) +
        (vector [2] * vector [2]));
    oldHyp = SQRT( t );
    vector [0] = vector [0] / oldHyp;
    vector [1] = vector [1] / oldHyp;
    vector [2] = vector [2] / oldHyp;
}

float dotProduct (float *a, float *b) {
    float ab;
    ab = (a[0] * b[0]) + ((a[1] * b[1]) + (a[2] * b[2]));
    return ab;
}

int findRange ( int a, int b, int c, int d ) {
    int max, min;
    if (a > b) {
        max = a;
        min = b;
    } else {
        max = b;
        min = a;
    }
    if (c > max) {
        max = c;
    } else if (c < min) {
        min = c;
    } else {
    }
    if (d > max) {
        max = d;
    } else if (d < min) {
        min = d;
    } else {

    }
    return max - min;
}

void initWORDvec (int *vec, int pattern, int words ) {
    int i;
    for (i=0; i < words; i++) {
        vec[i] = pattern;
    }
}

#define notRendered 0x80000000
#define maxDescend 2   /* this is a space limitation on the sub-pixel grid rather than a compute saver */
#define descendPower 4 /* 2 ^^ maxDescend */

#define threshold  10 << 2 /* this is 10 on a scale of 256, but we */
#define colourBits 10 /* generate 10 bits */
#define rMask (1<<colourBits)-1
#define gMask rMask<<colourBits
#define bMask gMask<<colourBits

typedef struct {
    void *objPtr;
    float sectx;
    float secty;
    float sectz;
    float normx;
    float normy;
    float normz;
    float t;
    float startx;
    float starty;
    float startz;
    float dx;
    float dy;
    float dz;
    float red;
    float green;
    float blue;     
} NODE;

NODE cleanTree[maxNodes];

NODE *tree = cleanTree;

int pointSample (int **patch, int patchx, int patchy, int x, int y ) {
    int colour = patch[y][x];
    if (colour == notRendered) {
        int tx, ty;
        float wx, wy;
        int iwx = wx;
        int iwy = wy;
        int treep;
        NODE node;
        tx = (patchx << maxDescend) + x;
        ty = (patchy << maxDescend) + y;

        wx = floor(tx) / floor(descendPower);
        wy = floor(ty) / floor(descendPower);

        treep = buildShadeTree (wx, wy);
        shade (treep);
        node = tree[treep];
        colour = (int)node.red | (int)node.green << colourBits | (int)node.blue << (colourBits + colourBits);
        patch[y][x] = colour;
    }
    return colour;
}

#define a_render 0
#define a_shade  1
#define a_stop   2

typedef struct {
    int hop;
    int y;
    int x;
    int action;
} SHADE;

typedef struct {
    int action;
    int hop;
    int y;
    int x;
    SHADE shade[4];
} STACK_ENTRY;

void renderPixels ( int patchx, int patchy,
                    int x0, int y0,
                    int **patch,
                    int *colour,
                    int patchSize,
                    int renderingMode,
                    int debug) {
    STACK_ENTRY stack[maxDescend * (4 * 17)];   /* 4 * (render, x, y, hop); shade */
    int colstack[maxDescend + 1];
    int sp, cp;
    int x, y, hop;
    int action;
    if (renderingMode == m_adaptive) {
        cp        = 0;                 /*-- empty colour stack */
        action    = a_render;          /* -- init action */
        sp = 0;                /*-- pre load stack with render x y hop; stop*/
        stack[sp].action = a_stop;
        stack[sp].hop = descendPower;      /*-- set grid hop value to gross pixel level*/
        stack[sp].y = y0 << maxDescend;
        stack[sp].x = x0 << maxDescend;  /*-- locations within this patch*/
        while (action != a_stop) {
            if (action == a_render) {
                int a, b, c, d, rRange, gRange, bRange;
                int sg = colourBits;
                int sb = colourBits + colourBits;
                STACK_ENTRY record = stack[sp];
                x   = record.x;
                y   = record.y;
                hop = record.hop;
                a = pointSample ( patch, patchx, patchy, x,       y      );
                b = pointSample ( patch, patchx, patchy, x + hop, y      );
                c = pointSample ( patch, patchx, patchy, x,       y + hop);
                d = pointSample ( patch, patchx, patchy, x + hop, y + hop);

                rRange = findRange (a & rMask, b & rMask,
                                    c & rMask, d & rMask);

                gRange = findRange ( (a & gMask) >> sg, (b & gMask) >> sg,
                                    (c & gMask) >> sg, (d & gMask) >> sg);

                bRange = findRange ( a >> sb, b >> sb,
                                    c >> sb, d >> sb);

                if (hop != 1 &&
                    ((rRange > threshold) ||
                    ((gRange > threshold) || (bRange > threshold)))) {
                        record = stack[sp];
                        record.action = a_shade;
                        hop = hop >> 1;
                        record.shade[0].hop = hop;
                        record.shade[0].y = y;
                        record.shade[0].x = x;
                        record.shade[0].action = a_render;
                        record.shade[1].hop = hop;
                        record.shade[1].y = y;
                        record.shade[1].x = x + hop;
                        record.shade[1].action = a_render;
                        record.shade[2].hop = hop;
                        record.shade[2].y = y + hop;
                        record.shade[2].x = x;
                        record.shade[2].action = a_render;
                        record.shade[3].hop = hop;
                        record.shade[3].y = y + hop;
                        record.shade[3].x = x + hop;
                        record.shade[3].action = a_render;
                    sp = sp + 1;
                } else {
                    int R, G, B, m;
                    m = rMask;
                    R = ((((a & m) + (b & m)) +
                            ((c & m) + (d & m))) >> 2) & m;
                    m = gMask;
                    G = ((((a & m) + (b & m)) +
                            ((c & m) + (d & m))) >> 2) & m;
                    m = bMask;
                    B = ((((a & m) + (b & m)) +
                            ((c & m) + (d & m))) >> 2) & m;
                    colstack[cp] = R | G | B;
                    cp = cp + 1;
                }
                sp     = sp - 1;
                action = stack [sp].action;
            } else if (action == a_shade) {
                int a, b, c, d, R, G, B, m;
                a  = colstack[0];
                b  = colstack[-1];
                c  = colstack[-2];
                d  = colstack[-3];
                m = rMask;
                R = ((((a & m) + (b & m)) +
                        ((c & m) + (d & m))) >> 2) & m;
                m = gMask;
                G = ((((a & m) + (b & m)) +
                        ((c & m) + (d & m))) >> 2) & m;
                m = bMask;
                B = ((((a & m) + (b & m)) +
                        ((c & m) + (d & m))) >> 2) & m;
                colstack[0] = R | G | B;
                cp = cp - 3;
                sp = sp - 1;
                action = stack[sp].action;
            }
        }
        *colour = colstack[cp-1];
    } else if (renderingMode == m_test) {
        *colour = (x0 + (y0 * patchSize));
    }
}

object objects[MAX_OBJECTS];
light lights[MAX_LIGHTS];

/* job calculates a slice of pixels - one instance per node */
/* 1. sends a 0 on req_out to indicate it needs work
   2. receives (on job_in) a DATCOM to setup parameters, then a c_render 
   3. sends (on rsl_out) a c_patch with the pixels
*/   
void job(Channel *req_out, Channel *job_in, Channel *rsl_out)
{
    object *po = objects;
    light *pl = lights;
    int loading_scene = 1;
    while (loading_scene) {
        int type;
        type = ChanInInt(job_in);
        switch (type) {
            case c_object:
            {
                object o;
                ChanIn(job_in,(char *)&o, sizeof(o));
                memcpy (po++, &o, sizeof(o));
            }
            break;
            case c_light:
            {
                light l;
                ChanIn(job_in,(char *)&l, sizeof(l));
                memcpy (pl++, &l, sizeof(l));
            }
            break;
            case c_runData:
            {
                rundata r;
                ChanIn(job_in,(char *)&r, sizeof(r));
            }
            break;
            case c_start:
                loading_scene = 0;
            break;
            default:
                while(1) { lon(); }
            break;
        }
    }
    loop
    {
        int type;
        /* 1. tell selector that we're ready to recieve work */
        ChanOutInt(req_out,0);
        type = ChanInInt(job_in);
        if (type == c_render)
        {
            int x,y,pixvec;
            int buf[MAXPIX];
            int *pbuf;
            render r;
            patch p;
            ChanIn(job_in,(char *)&r,sizeof(r));
            pbuf = buf;
            for (y = 0; y < r.h; y++) {
                for (x = 0; x < r.w; x++)
                {
                    *pbuf++ = r.x*r.y*640;
                }
            }
            p.x = r.x;
            p.y = r.y;
            p.worker = 0;   /* TODO */
            p.patchWidth = r.w;
            p.patchHeight = r.h;
            ChanOutInt(rsl_out,c_patch);
            ChanOut(rsl_out,(char *)&p,sizeof(p));
            ChanOut(rsl_out, buf, p.patchWidth*p.patchHeight*4);
        } else {
            while (1) {lon();}
        }
    }
}

/* Worker to pass messages to child nodes. Receives messages from the selector
   One instance per hard link
   posts on req_out (to the selector) every time it's ready for more work */
void buffer(Channel * req_out, Channel *buf_in, Channel *buf_out)
{
    int loading_scene = 1;
    while (loading_scene) {
        int type;
        type = ChanInInt(buf_in);
        switch (type){
            case c_object:
            {
                object o;
                ChanIn(buf_in,(char *)&o, sizeof(o));
                ChanOutInt(buf_out,type);
                ChanOut(buf_out,&o,sizeof(o));
            }
            break;
            case c_light:
            {
                light l;
                ChanIn(buf_in,(char *)&l, sizeof(l));
                ChanOutInt(buf_out,type);
                ChanOut(buf_out,&l,sizeof(l));
            }
            break;
            case c_runData:
            {
                rundata r;
                ChanIn(buf_in,(char *)&r, sizeof(r));
                ChanOutInt(buf_out,type);
                ChanOut(buf_out,&r,sizeof(r));
            }
            break;
            case c_start:
                ChanOutInt(buf_out,type);
                loading_scene = 0;
            break;
            default:
                while(1) {lon();}
                break;
        }
    }
    loop
    {
        int type;
        ChanOutInt(req_out,0);
        type = ChanInInt(buf_in);
        if (type == c_render)
        {
            render r;
            ChanIn(buf_in,(char *)&r, sizeof(r));
            ChanOutInt(buf_out, type);
            ChanOut(buf_out, (char *)&r, sizeof(r));
        } else {
            while(1) {lon();}
        }
    }
}

/* 1. waits for a buffer from the parent
   2. waits for a job() or a buffer() to inform they're ready (by writing a 0 on request channel)
   3. sends the buffer to the child that's ready */
void selector(Channel *sel_in, Channel **req_in, Channel **dn_out)
{
    int loading_scene = 1;
    while (loading_scene) {
        int type;
        type = ChanInInt(sel_in);
        switch (type) {
            case c_object:
            {
                object o;
                int i;
                ChanIn(sel_in,(char *)&o, sizeof(o));
                /* send the object to each worker */
                for (i=0; i < 4; i++) {
                    if (dn_out[i]!=(void *)0) {
                        ChanOutInt(dn_out[i],type);
                        ChanOut(dn_out[i],&o,sizeof(o));
                    }
                }
            }
            break;
            case c_light:
            {
                light l;
                int i;
                ChanIn(sel_in,(char *)&l, sizeof(l));
                /* send the object to each worker */
                for (i=0; i < 4; i++) {
                    if (dn_out[i]!=(void *)0) {
                        ChanOutInt(dn_out[i],type);
                        ChanOut(dn_out[i],&l,sizeof(l));
                    }
                }
            }
            break;
            case c_runData:
            {
                rundata r;
                int i;
                ChanIn(sel_in,(char *)&r, sizeof(r));
                /* send the object to each worker */
                for (i=0; i < 4; i++) {
                    if (dn_out[i]!=(void *)0) {
                        ChanOutInt(dn_out[i],type);
                        ChanOut(dn_out[i],&r,sizeof(r));
                    }
                }
            }
            break;
            case c_start:
            {
                int i;
                /* send the command to each worker */
                for (i=0; i < 4; i++) {
                    if (dn_out[i]!=(void *)0) {
                        ChanOutInt(dn_out[i],type);
                    }
                }
                loading_scene = 0;
            }
            break;
            default:
                while(1) {lon();}
            break;
        }
    }
    loop
    {
        /* 1. wait for a job from the parent */
        int type;
        type = ChanInInt(sel_in);
        switch (type) {
            case c_render:
            {
                render r;
                int i;
                ChanIn(sel_in,(char *)&r,sizeof(r));
                /* 2. wait for any worker to become ready */
                i = ProcPriAltList(req_in,0);
                ChanInInt(req_in[i]);       /* discard the 0 */
                /* 3. send the job to the worker */
                ChanOutInt(dn_out[i],type);
                ChanOut(dn_out[i],(char *)&r,sizeof(r));
            }
            break;
            default:
                /*while(1) {lon();}*/
            break;
        }
    }
}

/* This runs on the root node only. Split full image into MAXPIX (or less) sized jobs */
void feed(Channel *host_in, Channel *host_out, Channel *fd_out, int fxp)
{
    loop
    {
        int type;
        type = ChanInInt(host_in);
        switch (type) {
            case c_object:
            {
                object o;
                ChanIn(host_in,(char *)&o, sizeof(o));
                /* pass downstream */
                ChanOutInt (fd_out, type);
                ChanOut(fd_out,(char *)&o,sizeof(o));
                /* ack to host */
                ChanOutInt(host_out,c_object_ack);
            }
            break;
            case c_light:
            {
                light l;
                ChanIn(host_in,(char *)&l, sizeof(l));
                ChanOutInt (fd_out, type);
                ChanOut(fd_out,(char *)&l,sizeof(l));
                ChanOutInt(host_out,c_light_ack);
            }
            break;
            case c_runData:
            {
                rundata r;
                ChanIn(host_in,(char *)&r, sizeof(r));
                ChanOutInt (fd_out, type);
                ChanOut(fd_out,(char *)&r,sizeof(r));
                ChanOutInt(host_out,c_runData_ack);
            }
            break;
            case c_start:
            {
                ChanOutInt (fd_out, c_start);
                ChanOutInt(host_out,c_start_ack);
                {
                    int width,height;
                    int block_height, block_width;
                    render r;
                    width = 640;
                    height = 480;
                    block_width = BLOCK_WIDTH;
                    block_height = BLOCK_HEIGHT;

                    r.w = block_width;
                    r.h = block_height;
                    /* Dispatch render messages for each slice to the selector. The selector distributes the work */
                    for (r.y = 0; r.y < height; r.y+=block_height)
                    {
                        for (r.x = 0; r.x < width; r.x+=block_width)
                        {
                            ChanOutInt (fd_out, c_render);
                            ChanOut(fd_out,(char *)&r,sizeof(r));
                        }
                    }
                }
                /* inform host that work is finished */
                ChanOutInt(host_out,c_done);
            }
            break;
            default:
                while(1) {lon();}
            break;
        }
    }
}
