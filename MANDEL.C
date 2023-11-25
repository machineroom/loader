/******************************** MANDEL.C **********************************
*  (C) Copyright 1987-1993  Computer System Architects, Provo UT.           *
*  This  program is the property of Computer System Architects (CSA)        *
*  and is provided only as an example of a transputer/PC program for        *
*  use  with CSA's Transputer Education Kit and other transputer products.  *
*  You may freely distribute copies or modifiy the program as a whole or in *
*  part, provided you insert in each copy appropriate copyright notices and *
*  disclaimer of warranty and send to CSA a copy of any modifications which *
*  you plan to distribute.						    *
*  This program is provided as is without warranty of any kind. CSA is not  *
*  responsible for any damages arising out of the use of this program.      *
****************************************************************************/

/****************************************************************************
* This program mandel.c is written in Logical System's C and Transputer     *
* assembly. This program provides fuctions to calculate mandelbrot loop     *
* in a fixed point or floating point airthmatic.                            *
* This program is compiled and output is converted to binary hex array and  *
* is down loaded onto transputer network by program MAN.C                   *
****************************************************************************/
/* compile: tcx file -cf1p0rv */

#include <conc.h>
#include <math.h>

#include "common.h"
/*{{{  notes on compiling and linking*/
/*
This module is compiled as indicated above so it will be relocatable.
It is then linked with main (not _main) specified as the entry point.
This is because the fload loader loads the code in and thens jumps
to the first byte of the code read in.  By specifying main as the
entry point the code is correctly linked and the first instruction of
main will be the first byte in the code.
*/
/*}}}  */

/*{{{  define constants*/
#define TRUE  1
#define FALSE 0

#define JOBWSZ (53*4)+MAXPIX
#define BUFWSZ (20*4)
#define FEDWSZ (24*4*10)
#define ARBWSZ (22*4)+MAXPIX
#define SELWSZ (29*4)

#define THRESH 2.0e-7
#define loop for (;;)

#define   T805  0x0a
#define   T805L 0x9
#define   T805H	0x14
#define   T800D 0x60
#define   T414B 0x61
/*}}}  */
/*{{{  lights macro*/
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
/*}}}  */

/*{{{  struct type def shared with loader and ident asembler code */
typedef struct {
    int id;
    void *minint;
    void *memstart;
    Channel *up_in;
    Channel *dn_out[3];
    void *ldstart;
    void *entryp;
    void *wspace;
    void *ldaddr;
    void *trantype;
} LOADGB;

/*}}}  */
/*{{{  main(...)*/

main(ld)
LOADGB *ld;
{
    int i,fxp;

    /*{{{  get num nodes and whether floating point*/
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
    /*}}}  */
    }
    /*{{{  set up par structure*/
    {
        Channel *si,*so[5],*sr[5],*ai[5];
        extern int job(),buffer(),feed(),arbiter(),selector();
        extern int jobws[JOBWSZ/4];
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
                PRun(PSetupA(buffer,BUFWSZ,3,sr[list_index],so[list_index],ld->dn_out[i]));
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
        }
        else
        {
            /* id=0 == root node */
            si = ChanAlloc();
            PRun(PSetupA(feed,FEDWSZ,3,ld->up_in,si,fxp));
        }
        /* arbiter(Channel **arb_in, Channel *arb_out) */
        PRun(PSetupA(arbiter,ARBWSZ,2,ai,ld->up_in-4));
        /* selector(Channel *sel_in, Channel **req_in, Channel **dn_out) */
        PRun(PSetupA(selector,SELWSZ,3,si,sr,so));
        PStop();
    }
    /*}}}  */
}
/*}}}  */

/*{{{  job(...)*/
/* job calculates a slice of pixels - one instance per node */
/* 1. sends a 0 on req_out to indicate it needs work
   2. receives (on job_in) a DATCOM to setup parameters, then a JOBCOM 
   3. sends (on rsl_out) a RSLCOM with the pixels
*/   
int jobws[JOBWSZ/4];

job(req_out,job_in,rsl_out)
Channel *req_out,*job_in,*rsl_out;
{
    int (*iter)();
    int i,len,pixvec,maxcnt,fxp;
    int ilox,igapx,iloy,igapy;
    double dlox,dgapx,dloy,dgapy;
    int buf[RSLCOM_BUFSIZE];
    unsigned char *pbuf = (unsigned char *)(buf+3);

    loop
    {
        /* 1. tell selector that we're ready to recieve work */
        ChanOutInt(req_out,0);
        len = ChanInInt(job_in);
        ChanIn(job_in,(char *)buf,len);
        if (buf[0] == JOBCOM)
        /*{{{  */
        {
            pixvec = buf[3];
            if (fxp)
            /*{{{  */
            {
                int x,y;
                if (igapx && igapy)
                {
                    y = igapy*buf[2]+iloy;
                    for (i = 0; i < pixvec; i++)
                    {
                        x = igapx*(buf[1]+i)+ilox;
                        pbuf[i] = iterFIX(x,y,maxcnt);
                    }
                }
                else
                {
                    for (i = 0; i < pixvec; i++) pbuf[i] = 1;
                }
            }
            /*}}}  */
            else
            {
                double x,y;
                y = buf[2]*dgapy+dloy;
                for (i = 0; i < pixvec; i++)
                {
                    x = (buf[1]+i)*dgapx+dlox;
                    pbuf[i] = (*iter)(x,y,maxcnt);
                }
            }
            len = pixvec+3*4;
            buf[0] = RSLCOM;
        }
        /*}}}  */
        else if (buf[0] == DATCOM)
        /*{{{  */
        {
            fxp = buf[1];
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
                int iterR32(),iterR64();
                dlox = *(double *)(buf+3);
                dloy = *(double *)(buf+5);
                dgapx = *(double *)(buf+7);
                dgapy = *(double *)(buf+9);
                if (dgapx < THRESH || dgapy < THRESH) iter = iterR64;
                else  iter = iterR32;
            }
            continue;
        }
        /*}}}  */
        ChanOutInt(rsl_out,len);
        ChanOut(rsl_out,(char *)buf,len);
    }
}

/*}}}  */

/* fixed point iterate used by T4 variants */
/*{{{  int iterFIX(...)*/
int iterFIX(cx,cy,maxcnt)
int cx,cy,maxcnt;
{
    int cnt,zx,zy,zx2,zy2,tmp;

#pragma asm
    .VAL    maxcnt, ?ws+3
        .VAL    cy,     ?ws+2
        .VAL    cx,     ?ws+1
        .VAL    tmp,    ?ws-1
        .VAL    zy2,    ?ws-2
        .VAL    zx2,    ?ws-3
    .VAL    zy,     ?ws-4
        .VAL    zx,     ?ws-5
        .VAL    cnt,    ?ws-6
    ; zx = zy = zx2 = zy2 = cnt = 0;
        LDC     0
        STL     zx
    LDC     0
        STL     zy
        LDC     0
        STL     zx2
        LDC     0
        STL     zy2
    LDC     0
        STL     cnt
LOOP:
    ; cnt++;
        LDL     cnt
        ADC     1
    STL     cnt
        ; if (cnt >= maxcnt) goto END;
        LDL     maxcnt
        LDL     cnt
        GT
        CJ      @END
    ; tmp = zx2 - zy2 + cx;
        LDL     zx2
        LDL     zy2
    SUB
        LDL     cx
        ADD
    STL     tmp
        ; zy = zx * zy * 2 + cy;
        LDL     zx
        LDL     zy
        FMUL
        XDBLE
    LDC 3
        LSHL
        CSNGL
    LDL     cy
        ADD
        STL     zy
    ; tmp = zx;
        LDL     tmp
        STL     zx
        ; zx2 = zx * zx;
        LDL     zx
        LDL     zx
    FMUL
        XDBLE
        LDC 2
    LSHL
        CSNGL
        STL     zx2
    ; zy2 = zy * zy;
        LDL     zy
        LDL     zy
        FMUL
        XDBLE
        LDC     2
    LSHL
        CSNGL
        STL     zy2
    ; if (zx2 + zy2 >= 4.0) goto END;
        LDL     zx2
        LDL     zy2
    ADD
        TESTERR
        CJ      @END
        J       @LOOP
END:
        ; if (cnt != maxcnt) return(cnt);
    LDL     cnt
        LDL     maxcnt
        DIFF
    CJ      @RETN
        LDL     cnt
        .RETF   ?ws
RETN:
        ; return(0);
        .RETF   ?ws
#pragma endasm
}

/* ONLY used by T8 variants */
/*}}}  */
/*{{{  int iterR64(...)*/
int iterR64(cx,cy,maxcnt)
double cx,cy;
int maxcnt;
{
    int cnt;
    double zx,zy,zx2,zy2,tmp,four;

    four = 4.0;
    zx = zy = zx2 = zy2 = 0.0;
    cnt = 0;
    do
    {
        tmp = zx2-zy2+cx;
        zy = zx*zy*2.0+cy;
        zx = tmp;
        zx2 = zx*zx;
        zy2 = zy*zy;
        cnt++;
    }
    while (cnt < maxcnt && zx2+zy2 < four);
    if (cnt != maxcnt) return(cnt);
    return(0);
}

/* ONLY used by T8 variants */
/*}}}  */
/*{{{  int iterR32(...)*/
int iterR32(cx,cy,maxcnt)
double cx,cy;
int maxcnt;
{
    int cnt;
    float x,y,zx,zy,zx2,zy2,tmp,four;

    four = 4.0f;
    zx = zy = zx2 = zy2 = 0.0f;
    x = cx; y = cy;
    cnt = 0;
    do
    {
        tmp = zx2-zy2+x;
        zy = zx*zy*2.0f+y;
        zx = tmp;
        zx2 = zx*zx;
        zy2 = zy*zy;
        cnt++;
    }
    while (cnt < maxcnt && zx2+zy2 < four);
    if (cnt != maxcnt) return(cnt);
    return(0);
}

/*}}}  */

/* cast a float to fixed point variant? */
/*{{{  int fix(x)*/
int fix(x)
int *x;
{
    int e,tmp;

#pragma asm
        .VAL    x,?ws+1
        .VAL    tmp,?ws-1
    .VAL    e,?ws-2
        ; e = ((x[1] >> 20) & 0x7ff) - 1023 + 9;
        LDL     x
    LDNL    1
        .LDC    20
        SHR
        .LDC    0x7ff
        AND
        ADC     (-1023)+(+9)
    STL     e
        ; reg = (x[1] & 0xfffff) | 0x100000;
        LDL     x
    LDNL    1
        .LDC    0xfffff
        AND
    .LDC    0x100000
        OR
        ; if (e < 0) { F2;
        .LDC    0
        LDL     e
        GT
    CJ      @F2
        ; if (e > -21) { F1;
        LDL     e
    .LDC    -21
        GT
        CJ      @F1
    ; reg >>= -e;
        LDL     e
        NOT
        ADC     1
        SHR
F1:
    ; } else reg = 0;
        .LDC    0
        CJ      @F4
F2:
        ; } else if (e < 11) { F3;
        ADD
    .LDC    +11
        LDL     e
        GT
        CJ      @F3
        ; lreg <<= e;
        LDL     x
    LDNL    0
        LDL     e
        LSHL
    STL     tmp
        .LDC    0
        CJ      @F4
F3:
        ; } else reg = 0x7fffffff;
        .LDC    0x7fffffff
        .LDC    0
F4:
        ; if (x[1] < 0) return(-reg);
    LDL     x
        LDNL    1
        GT
    CJ      @F5
        NOT
        ADC     1
    .RETF   ?ws
F5:
        ; return(reg);
        ADD
        .RETF   ?ws
#pragma endasm
}

/*}}}  */
/*{{{  buffer(...)*/
/* Worker to pass messages to child nodes. Receives messages from the selector
   One instance per hard link
   posts on req_out (to the selector) every time it's ready for more work */
buffer(req_out,buf_in,buf_out)
Channel *req_out,*buf_in,*buf_out;
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

/*}}}  */
/*{{{  arbiter(...)*/
/* arb_in is list of channels connected to job or buffer processes */
/* arb_out is the output side of the parent link */
/* Receives result from worker (local job or link buffer processes) and passes result to parent */
arbiter(arb_in,arb_out)
Channel **arb_in,*arb_out;
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
        if (buf[0] == FLHCOM) {
            if (arb_in[++cnt]) 
                continue; 
            else 
                cnt = 0;
        }
        /* 3. Send result to parent */
        ChanOutInt(arb_out,len);
        ChanOut(arb_out,(char *)buf,len);
    }
}

/*}}}  */
/*{{{  selector(...)*/
/* 1. waits for a buffer from the parent
   2. waits for a job() or a buffer() to inform they're ready (by writing a 0 on request channel)
   3. sends the buffer to the child that's ready */
selector(sel_in,req_in,dn_out)
Channel *sel_in,**req_in,**dn_out;
{
    int i,len;
    int buf[PRBSIZE-1];

    loop
    {
        /* 1. wait for a job from the parent */
        len = ChanInInt(sel_in);
        ChanIn(sel_in,(char *)buf,len);
        if (buf[0] == JOBCOM)
        /*{{{  */
        {
            /* 2. wait for any worker to become ready */
            i = ProcPriAltList(req_in,0);
            ChanInInt(req_in[i]);       /* discard the 0 */
            /* 3. send the job to the worker */
            ChanOutInt(dn_out[i],len);
            ChanOut(dn_out[i],(char *)buf,len);
        }
        /*}}}  */
        else
        /*{{{  */
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
        /*}}}  */
    }
}

#define mon_frequency       (unsigned int)25
#define mon_lineTime        (unsigned int)202
#define mon_halfSync        (unsigned int)8
#define mon_backPorch       (unsigned int)20
#define mon_display         (unsigned int)160
#define mon_shortDisplay    (unsigned int)61
#define mon_vDisplay        (unsigned int)960
#define mon_vBlank          (unsigned int)80
#define mon_vSync           (unsigned int)4
#define mon_vPreEqualize    (unsigned int)4
#define mon_vPostEqualize   (unsigned int)4
#define mon_broadPulse      (unsigned int)75
#define mon_memInit         (unsigned int)128
#define mon_xferDelay       (unsigned int)1
#define mon_lineStart       (unsigned int)0

#define IMS_332_REGBOOT	                (unsigned int)0
#define	IMS_332_REGHALFSYNCH            (unsigned int)0x21
#define	IMS_332_REGBACKPORCH            (unsigned int)0x22
#define	IMS_332_REGDISPLAY              (unsigned int)0x23
#define	IMS_332_REGSHORTDIS             (unsigned int)0x24
#define	IMS_332_REGBROADPULSE           (unsigned int)0x25
#define	IMS_332_REGVSYNC	            (unsigned int)0x26
#define	IMS_332_REGVPREEQUALIZE	        (unsigned int)0x27
#define	IMS_332_REGVPOSTEQUALIZE        (unsigned int)0x28
#define	IMS_332_REGVBLANK               (unsigned int)0x29
#define	IMS_332_REGVDISPLAY             (unsigned int)0x2A
#define	IMS_332_REGLINETIME             (unsigned int)0x2B
#define	IMS_332_REGLINESTART	        (unsigned int)0x2C
#define	IMS_332_REGMEMINIT	            (unsigned int)0x2D
#define	IMS_332_REGXFERDELAY	        (unsigned int)0x2E
#define IMS_332_REGCOLORMASK            (unsigned int)0x40
#define IMS_332_REGCSRA	                (unsigned int)0x60
#define IMS_332_REGCSRB	                (unsigned int)0x70
#define IMS_332_REGLUTBASE              (unsigned int)0x100 
#define IMS_332_REGLUTEND               (unsigned int)0x1FF

#define IMS_332_CSRA_VTGENABLE		    (unsigned int)0x000001	/* vertical timing generator */
#define IMS_332_CSRA_INTERLACED	    	(unsigned int)0x000002	/* screen format */
#define IMS_332_CSRA_CCIR		        (unsigned int)0x000004	/* default is EIA */
#define IMS_332_CSRA_SLAVESYNC		    (unsigned int)0x000008	/* else from our pll */
#define IMS_332_CSRA_PLAINSYNC		    (unsigned int)0x000010	/* else tesselated */
#define IMS_332_CSRA_SEPARATESYNC	    (unsigned int)0x000020	/* else composite */
#define IMS_332_CSRA_VIDEOONLY		    (unsigned int)0x000040	/* else video+sync */
#define IMS_332_CSRA_BLANKPEDESTAL	    (unsigned int)0x000080	/* blank level */
#define IMS_332_CSRA_CBLANKISOUT	    (unsigned int)0x000100
#define IMS_332_CSRA_CBLANKNODELAY	    (unsigned int)0x000200
#define IMS_332_CSRA_FORCEBLANK	        (unsigned int)0x000400
#define IMS_332_CSRA_BLANKDISABLE	    (unsigned int)0x000800
#define IMS_332_CSRA_VRAMINCREMENT	    (unsigned int)0x003000
#define IMS_332_VRAMINC1	            (unsigned int)0x000000
#define IMS_332_VRAMINC256	            (unsigned int)0x001000	/* except interlaced->2 */
#define IMS_332_VRAMINC512	            (unsigned int)0x002000
#define IMS_332_VRAMINC1024	            (unsigned int)0x003000
#define IMS_332_CSRA_DMADISABLE	        (unsigned int)0x004000
#define IMS_332_CSRA_SYNCDELAYMASK	    (unsigned int)0x038000	/* 0-7 VTG clk delays */
#define IMS_332_CSRA_PIXELINTERLEAVE	(unsigned int)0x040000
#define IMS_332_CSRA_DELAYEDSAMPLING	(unsigned int)0x080000
#define IMS_332_CSRA_BITSPERPIXEL   	(unsigned int)0x700000
#define IMS_332_CSRA_DISABLECURSOR	    (unsigned int)0x800000

#define IMS_335_BPP24		            (unsigned int)0x600000 

#define	IMS_332_BOOTPLL                 (unsigned int)0x00001F	/* xPLL, binary */
#define	IMS_332_BOOTCLOCKPLL            (unsigned int)0x000020	/* else xternal */
#define IMS_332_BOOT64BITMODE           (unsigned int)0x000040	/* else 32 */

void resetB438(void) {
    volatile unsigned int *boardRegBase = (unsigned int *)0x200000;
    *boardRegBase = 0;
    *boardRegBase = 1;
    *boardRegBase = 0;
}

void IMS_332_WriteRegister (int regno, unsigned int val) {
    volatile unsigned int *IMS332RegBase = (unsigned int *)0x0;
    volatile unsigned int *reg = IMS332RegBase;
    reg += regno;
    *reg = val;
}

void IMS_332_Init(void) {
    /* 
    PLL multipler in bits 0..4 (values from 5 to 31 allowed)
    // B438 TRAM derives clock from TRAM clock (5MHz)
    // SU are *2 for 24bpp mode
    */
    unsigned int clock;
    unsigned int pllMultiplier;
    unsigned int CSRA;
    clock = 5; /* 5MHz from TRAM */
    pllMultiplier = mon_frequency/clock;
    pllMultiplier = pllMultiplier * 2; /* 24bpp interleaved mode -> clock=2*dot rate */
    IMS_332_WriteRegister (IMS_332_REGBOOT,         pllMultiplier | IMS_332_BOOTCLOCKPLL);
    IMS_332_WriteRegister (IMS_332_REGCSRA,         0);
    IMS_332_WriteRegister (IMS_332_REGCSRB,         0xB); /* 1011 Split SAM, Sync on green, External pixel sampling */
    IMS_332_WriteRegister (IMS_332_REGLINETIME,	    mon_lineTime*2);
    IMS_332_WriteRegister (IMS_332_REGHALFSYNCH,    mon_halfSync*2);
    IMS_332_WriteRegister (IMS_332_REGBACKPORCH,    mon_backPorch*2);
    IMS_332_WriteRegister (IMS_332_REGDISPLAY,      mon_display*2);
    IMS_332_WriteRegister (IMS_332_REGSHORTDIS,	    mon_shortDisplay*2);
    IMS_332_WriteRegister (IMS_332_REGVDISPLAY,     mon_vDisplay);
    IMS_332_WriteRegister (IMS_332_REGVBLANK,	    mon_vBlank);
    IMS_332_WriteRegister (IMS_332_REGVSYNC,		mon_vSync);
    IMS_332_WriteRegister (IMS_332_REGVPREEQUALIZE, mon_vPreEqualize);
    IMS_332_WriteRegister (IMS_332_REGVPOSTEQUALIZE,mon_vPostEqualize);
    IMS_332_WriteRegister (IMS_332_REGBROADPULSE,	mon_broadPulse*2);
    IMS_332_WriteRegister (IMS_332_REGMEMINIT, 	    mon_memInit*2);
    IMS_332_WriteRegister (IMS_332_REGXFERDELAY,	mon_xferDelay*2);
    IMS_332_WriteRegister (IMS_332_REGLINESTART,	mon_lineStart*2);
    IMS_332_WriteRegister (IMS_332_REGCOLORMASK,    0xFFFFFFFF);
    CSRA = 0;
    CSRA |= IMS_332_CSRA_DISABLECURSOR;
    CSRA |= IMS_335_BPP24;
    CSRA |= IMS_332_CSRA_PIXELINTERLEAVE;
    CSRA |= IMS_332_VRAMINC1024;
    CSRA |= IMS_332_CSRA_PLAINSYNC;
    CSRA |= IMS_332_CSRA_VTGENABLE;
    CSRA |= IMS_332_CSRA_SEPARATESYNC;
    CSRA |= IMS_332_CSRA_VIDEOONLY;
    IMS_332_WriteRegister(IMS_332_REGCSRA, CSRA);
}

void poke_words (unsigned int addr, int count, unsigned int val) {
    const unsigned int VRAMHardware = 0x80400000;
    volatile unsigned int *a = (unsigned int *)VRAMHardware;
    a += addr;
    while (count--) {
        *a++ = val;
    }
}

/* Thanks again, Inmos. Somewhere deep in the data sheet is a one liner about the palette being used for
// gamma correction in true colour modes.
//some sort of gamma correction*/
void bt709Gamma (void) {
  unsigned int val, corrected;
  float g,i;
  int c;
  for (c=0; c < 256; c++) {
    /*i = (float)c;*/
    /*g = powf(i/255.0, 2.2);*/
    corrected = 255;/*(unsigned int)(255.0 * g);*/
    val = corrected;
    val <<= 8;
    val |= corrected;
    val <<= 8;
    val |= corrected;
    IMS_332_WriteRegister (IMS_332_REGLUTBASE + c, val);
  }
}
      
void setupGfx(void) {
    resetB438();
    IMS_332_Init();
    /*bt709Gamma();*/
    poke_words(0, 640*480, 0);
    poke_words(0,640*20,0xFF);/*blue*/
    poke_words(640*20,640*20,0xFF00);/*green*/
    poke_words(640*40,640*20,0xFF0000);/*red*/
    poke_words(640*60,640*20,0xFF00FF);/*pink*/
}


/*}}}  */
/*{{{  feed(...)*/
/* This runs on the root node only. Split full image into MAXPIX (or less) sized jobs */
feed(fd_in,fd_out,fxp)
Channel *fd_in,*fd_out;
int fxp;
{
    int len;
    int buf[PRBSIZE];
    setupGfx();
    loop
    {
        len = ChanInInt(fd_in);
        ChanIn(fd_in,(char *)buf,len);
        if (buf[0] == PRBCOM)
        /*{{{  */
        {
            int width,height,multiple;
            width = buf[1];
            height = buf[2];
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
            buf[0] = FLHCOM;
            len = 1*4;
        }
        /*}}}  */
        /* inform host that work is finished */
        ChanOutInt(fd_out,len);
        ChanOut(fd_out,(char *)buf,len);
    }
}
/*}}}  */
