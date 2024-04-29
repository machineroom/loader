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

#define JOBWSZ (53*4)+MAXPIX
#define BUFWSZ (20*4)
#define FEDWSZ (sizeof(object))+100
#define ARBWSZ (22*4*10)+MAXPIX
#define SELWSZ (29*4)

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

void *get_ws(int sz);


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
    {
        Channel *si,*so[5],*sr[5],*ai[5];
        extern void job(),buffer(),feed(),arbiter(),selector();
        extern int jobws[JOBWSZ];
        int list_index;

        /* job worker on each node - this does iter() */
        sr[0] = ChanAlloc();    /* sr = selector request (job request channel) */
        so[0] = ChanAlloc();    /* so = slector outputs (job configuration channel) */
        ai[0] = ChanAlloc();    /* ai = arbiter inputs (job/buffer results written to these channels) */
        /* job(Channel *req_out, Channel *job_in, Channel *rsl_out) */
        PRun(PSetup(jobws,job,JOBWSZ,3,sr[0],so[0],ai[0])|1);
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
                PRun(PSetup(get_ws(BUFWSZ),buffer,BUFWSZ,3,sr[list_index],so[list_index],ld->dn_out[i]));
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
            /* arbiter(Channel **arb_in, Channel *arb_out) */
            PRun(PSetup(get_ws(ARBWSZ),arbiter,ARBWSZ,3,ai,ld->up_in-4,0));
        }
        else
        {
            /* id=0 == root node */
            si = ChanAlloc();
            /* feed(Channel *host_in, Channel *host_out, Channel *fd_out, int fxp) */
            PRun(PSetup(get_ws(FEDWSZ),feed,FEDWSZ,4,ld->up_in,ld->up_in-4,si,fxp));
            /* arbiter(Channel **arb_in, Channel *arb_out) */
            PRun(PSetup(get_ws(ARBWSZ),arbiter,ARBWSZ,3,ai,ld->up_in-4,1));
        }
        /* selector(Channel *sel_in, Channel **req_in, Channel **dn_out) */
        PRun(PSetup(get_ws(SELWSZ),selector,SELWSZ,3,si,sr,so));
        PStop();
    }
}

/* Get a process workspace */
void *get_ws(int sz)
{
    void *rval;
    rval = malloc((size_t)sz);
    if (rval == NULL) {
        lon();
        exit(1);
    }
    return (rval);
}

/* job calculates a slice of pixels - one instance per node */
/* 1. sends a 0 on req_out to indicate it needs work
   2. receives (on job_in) a DATCOM to setup parameters, then a JOBCOM 
   3. sends (on rsl_out) a RSLCOM with the pixels
*/   
int jobws[JOBWSZ];

void job(Channel *req_out, Channel *job_in, Channel *rsl_out)
{
    int len;
    int buf[RSLCOM_BUFSIZE];

    loop
    {
        /* 1. tell selector that we're ready to recieve work */
        ChanOutInt(req_out,0);
        len = ChanInInt(job_in);
        ChanIn(job_in,(char *)buf,len);
        if (buf[0] == JOBCOM)
        {
            int i,x,y,pixvec;
            int *pbuf = &buf[3];
            x = buf[1];
            y = buf[2];
            pixvec = buf[3];
            for (i = 0; i < pixvec; i++)
            {
                pbuf[i] = 0xFF00+x+i;
            }
            len = (pixvec+3)*4;
            /* 0=RSLCOM
                1=x
                2=y
                3=pixels*/
            buf[0] = RSLCOM;
        }
        else if (buf[0] == DATCOM)
        {
            /*fxp = buf[1];
            maxcnt = buf[2];
            if (fxp)
            {
                ilox = fix(buf+3);
                iloy = fix(buf+5);
                igapx = fix(buf+7);
                igapy = fix(buf+9);
            }
            else
            {
                dlox = *(double *)(buf+3);
                dloy = *(double *)(buf+5);
                dgapx = *(double *)(buf+7);
                dgapy = *(double *)(buf+9);
                if (dgapx < THRESH || dgapy < THRESH) iter = iterR64;
                else  iter = iterR32;
            }*/
            continue;
        }
        ChanOutInt(rsl_out,len);
        ChanOut(rsl_out,(char *)buf,len);
    }
}

/* Worker to pass messages to child nodes. Receives messages from the selector
   One instance per hard link
   posts on req_out (to the selector) every time it's ready for more work */
void buffer(Channel * req_out, Channel *buf_in, Channel *buf_out)
{
    int len;
    int buf[PRBSIZE-1];

    loop
    {
        ChanOutInt(req_out,0);
        len = ChanInInt(buf_in);
        ChanIn(buf_in,(char *)buf,len);
        ChanOutInt(buf_out,len);
        ChanOut(buf_out,(char *)buf,len);
    }
}

/* arb_in is list of channels connected to job or buffer processes */
/* arb_out is the output side of the parent link */
/* Receives result from worker (local job or link buffer processes) and passes result to parent */
void arbiter(Channel **arb_in, Channel *arb_out, int root)
{
    int i,len,cnt,pri;
    int buf[RSLCOM_BUFSIZE];

    cnt = pri = 0;
    loop
    {
        /* 1. Wait for worker to notify result */
        i = ProcPriAltList(arb_in,pri);
        pri = (arb_in[pri+1]) ? pri+1 : 0;
        len = ChanInInt(arb_in[i]);
        ChanIn(arb_in[i],(char *)buf,len);
        if (root) {
            if (buf[0] == RSLCOM) {
                /*write_pixels (buf[0], buf[1], len-2, &buf[2]);*/
                /* 0=RSLCOM
                   1=x
                   2=y
                   3=pixels*/
                {
                    int *a = (int *)0x80400000;
                    int i;
                    int *pixels = &buf[3];
                    int count = (len/4)-3;  /* len is bytes */
                    a += (buf[2]*640)+buf[1];
                    for (i=0; i < count; i++) {
                        *a++ = pixels[i];
                    }
                }
           } else {
                ChanOutInt(arb_out,len);
                ChanOut(arb_out,(char *)buf,len);
           }
        } else {
            /* 3. Send result to parent */
            ChanOutInt(arb_out,len);
            ChanOut(arb_out,(char *)buf,len);
        }
    }
}

/* 1. waits for a buffer from the parent
   2. waits for a job() or a buffer() to inform they're ready (by writing a 0 on request channel)
   3. sends the buffer to the child that's ready */
void selector(Channel *sel_in, Channel **req_in, Channel **dn_out)
{
    int i,len;
    int buf[PRBSIZE-1];

    loop
    {
        /* 1. wait for a job from the parent */
        len = ChanInInt(sel_in);
        ChanIn(sel_in,(char *)buf,len);
        if (buf[0] == JOBCOM)
        {
            /* 2. wait for any worker to become ready */
            i = ProcPriAltList(req_in,0);
            ChanInInt(req_in[i]);       /* discard the 0 */
            /* 3. send the job to the worker */
            ChanOutInt(dn_out[i],len);
            ChanOut(dn_out[i],(char *)buf,len);
        }
        else
        {
            /* non-JOB messages are distributed round-robin to each worker (job or buffer process) */
            for (i = 0; req_in[i]; i++)
            {
                /* 2. wait for the worker to become ready */
                ChanInInt(req_in[i]);   /* discard the 0 */
                /* 3. send the message to the worker */
                ChanOutInt(dn_out[i],len);
                ChanOut(dn_out[i],(char *)buf,len);
            }
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
                ChanOutInt(host_out,c_object_ack);
            }
            break;
            case c_light:
            {
                light l;
                ChanIn(host_in,(char *)&l, sizeof(l));
                ChanOutInt(host_out,c_light_ack);
            }
            break;
            case c_runData:
            {
                rundata r;
                ChanIn(host_in,(char *)&r, sizeof(r));
                ChanOutInt(host_out,c_runData_ack);
            }
            break;
            case c_start:
            {
                ChanOutInt(host_out,c_start_ack);
                {
                    int width,height,multiple;
                    int buf[20];
                    width = 640;
                    height = 480;
                    /* abuse buf to send parameters in a DATCOM block */
                    /* Dispatch single DATCOM block to each worker */
                    buf[1] = DATCOM;
                    buf[2] = fxp;
                    ChanOutInt(fd_out,(PRBSIZE-1)*4);
                    ChanOut(fd_out,(char *)&buf[1],(PRBSIZE-1)*4);
                    buf[0] = JOBCOM;
                    /* abuse buf to send parameters in a JOBCOM block */
                    /* Dispatch JOBCOM blocks for each slice to the selector. The selector distributes the work */
                    multiple = width/MAXPIX*MAXPIX;
                    for (buf[2] = 0; buf[2] < height; buf[2]++)
                    {
                        for (buf[1] = 0; buf[1] < width; buf[1]+=MAXPIX)
                        {
                            buf[3] = (buf[1] < multiple) ? MAXPIX : width-multiple;
                            ChanOutInt(fd_out,4*4);
                            ChanOut(fd_out,(char *)buf,4*4);
                        }
                    }
                }
                /* inform host that work is finished */
                ChanOutInt(host_out,c_done);
            }
            break;
        }
    }
}
