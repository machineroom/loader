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

#ifdef NATIVE
    #include <stdio.h>
    #include "conc_native.h"
#else
    #include <conc.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "mlibp.h"
#include "loader.h"

#include "rcommon.h"
#include "B438.h"

/*  define constants*/
#define TRUE  1
#define FALSE 0

#define loop for (;;)

#ifdef NATIVE
    void lon(){}
    void loff(){}
#else
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
#endif


#define JOBWSZ 8192+MAXPIX
#define FEDWSZ 4048
#define SELWSZ 4048
#define BUFWSZ 4048
#define ARBWSZ 4048+MAXPIX
char stack[64*1024];    /* LSC puts stack at 0x80001000 then jobws at same location. Add some padding for real stack */

int jobws[JOBWSZ];
char stack1[64*1024];    /* LSC puts stack at 0x80001000 then jobws at same location. Add some padding for real stack */
int bufws[3][BUFWSZ];
char stack2[64*1024];    /* LSC puts stack at 0x80001000 then jobws at same location. Add some padding for real stack */
int selws[SELWSZ];
char stack3[64*1024];    /* LSC puts stack at 0x80001000 then jobws at same location. Add some padding for real stack */
int feedws[FEDWSZ];
char stack4[64*1024];    /* LSC puts stack at 0x80001000 then jobws at same location. Add some padding for real stack */
int arbws[ARBWSZ];
char stack5[64*1024];    /* LSC puts stack at 0x80001000 then jobws at same location. Add some padding for real stack */

/* NOTE main() must be first function */

#ifdef NATIVE
void transputer_main (void *host_in, void *host_out)
#else
main(LOADGB *ld)
#endif
{
    int i,fxp=0;
#ifdef NATIVE
    LOADGB _ld;
    // 0 nodes (just root)
    _ld.id = 0;
    _ld.minint = (void *)0x80000000;
    _ld.dn_out[0] = NULL;
    _ld.dn_out[1] = NULL;
    _ld.dn_out[2] = NULL;
    _ld.up_in = (Channel *)host_out;
    LOADGB *ld = &_ld;
    Channel *up_out = (Channel *)host_in;
#else
    Channel *up_out = ld->up_in-4;
    if (ld->id == 0) {
        setupGfx(1, 1);
    }
#endif

    /* get num nodes and whether floating point*/
    {
        int nodes;
        int buf[2];

        nodes = fxp = 0;
#ifndef NATIVE
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
#endif
        buf[0] = nodes+1;
        buf[1] = fxp;
        ChanOut(up_out,(char *)buf,2*4);
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
        #ifdef NATIVE
            void job(Channel *req_out, Channel *job_in, Channel *rsl_out);
            void buffer(Channel * req_out, Channel *buf_in, Channel *buf_out);
            void feed(Channel *host_in, Channel *host_out, Channel *fd_out, void *fxp);
            void arbiter(Channel **arb_in, Channel *arb_out, void *root);
            void selector(Channel *sel_in, Channel **req_in, Channel **dn_out);
        #else
            extern void job(),buffer(),feed(),arbiter(),selector();
        #endif
        int list_index;
        void *root;

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
            root = (void *)0;
        }
        else
        {
            /* id=0 == root node */
            si = ChanAlloc();
            /* feed(Channel *host_in, Channel *host_out, Channel *fd_out, int fxp) */
            PRun(PSetup(feedws,feed,FEDWSZ,4,ld->up_in,up_out,si,(void *)fxp));
            root = (void *)1;
        }
        /* arbiter(Channel **arb_in, Channel *arb_out, void * root) */
        PRun(PSetup(arbws,arbiter,ARBWSZ,3,ai,up_out,root));
        /* selector(Channel *sel_in, Channel **req_in, Channel **dn_out) */
        PRun(PSetup(selws,selector,SELWSZ,3,si,sr,so));
        ChanOutInt(up_out,c_ready);
        PStop();
    }
}

/* arb_in is list of channels connected to job or buffer processes */
/* arb_out is the output side of the parent link */
/* Receives result from worker (local job or link buffer processes) and passes result to parent */
void arbiter(Channel **arb_in, Channel *arb_out, void *root)
{
    int i,len,cnt,pri;
    int buf[MAXPIX];
    int type;

    cnt = pri = 0;
    loop
    {
        /* 1. Wait for worker to notify result */
        i = ProcAltList(arb_in);
        type = ChanInInt(arb_in[i]);
        switch (type) {
            case c_patch:
            {
                patch p;
                ChanIn(arb_in[i],(char *)&p,(int)sizeof(p));
                ChanIn(arb_in[i],buf,p.patchWidth*p.patchHeight*4);
                if (root==(void *)1) {
                    #ifdef NATIVE
                    int *pbuf = buf;
                    int y;
                    for (y=0; y < p.patchHeight; y++) {
                        write_pixels (p.x,p.y+y,8,pbuf);
                        pbuf+=8;
                    }
                    #else
                    /* render to B438 */
                    int *a = (int *)0x80400000;
                    int *pbuf = buf;
                    int x,y;
                    i = 0;
                    a += (p.y*640)+p.x;
                    for (y=0; y < p.patchHeight; y++) {
                        for (x=0; x < p.patchWidth; x++) {
                            a[x] = buf[i++];
                        }
                        a += 640;
                    }
                    #endif
                } else {
                    /* 3. Send result to parent */
                    ChanOutInt(arb_out,type);
                    ChanOut(arb_out,(char *)&p,(int)sizeof(p));
                    ChanOut(arb_out,(char *)buf,p.patchWidth*p.patchHeight*4);
                }
            }
            break;
            case c_message:
                {
                    int size;
                    char buf[1024]={0};
                    size = ChanInInt (arb_in[i]);
                    ChanIn(arb_in[i], buf, size);
                    ChanOutInt (arb_out, type);
                    ChanOutInt (arb_out, size);
                    ChanOut (arb_out, buf, size);
                }
            break;
            case c_message2:
                {
                    int size;
                    int p1, p2;
                    char buf[1024]={0};
                    p1 = ChanInInt (arb_in[i]);
                    p2 = ChanInInt (arb_in[i]);
                    size = ChanInInt (arb_in[i]);
                    ChanIn(arb_in[i], buf, size);
                    ChanOutInt (arb_out, type);
                    ChanOutInt (arb_out, p1);
                    ChanOutInt (arb_out, p2);
                    ChanOutInt (arb_out, size);
                    ChanOut (arb_out, buf, size);
                }
            break;
            default:
/*                ChanOutInt(arb_out,c_message2);
                ChanOutInt(arb_out,type);
                ChanOutInt(arb_out, i);
                ChanOutInt(arb_out,11);
                ChanOut(arb_out,"ARB BAD CMD",11);*/
                lon();
            break;
        }
    }
}

void Debug (Channel *out, char *message, int p1, int p2) {
    int l = strlen(message);
    ChanOutInt (out, c_message2);
    ChanOutInt (out, p1);
    ChanOutInt (out, p2);
    ChanOutInt (out, l);
    ChanOut (out, message, l);
}

object objects[MAX_OBJECTS];    /* replaces worldModel in occam */
light lights[MAX_LIGHTS];
_rundata rundata;
int num_objects=0;
int num_lights=0;

void normalize ( float *vector, float *oldHyp ) {
    float t;
    t = (vector [0] * vector [0]) +
        ((vector [1] * vector [1]) +
         (vector [2] * vector [2]));
    *oldHyp = sqrtf( t );
    vector [0] = vector [0] / *oldHyp;
    vector [1] = vector [1] / *oldHyp;
    vector [2] = vector [2] / *oldHyp;
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

#define maxDescend 2   /* this is a space limitation on the sub-pixel grid rather than a compute saver */
#define descendPower 4 /* 2 ^^ maxDescend */

#define PATCH_SIZE 16
#define GRID_SIZE ((PATCH_SIZE + 1) * descendPower) + 1
#define threshold  10 << 2 /* -- this is 10 on a scale of 256, but we generate 10 bits */
#define colourBits 10 /* xxbbbbbbbbbbggggggggggrrrrrrrrrr */
#define rMask (1<<colourBits)-1
#define gMask rMask<<colourBits
#define bMask gMask<<colourBits
#define nil -1
#define mint 0x80000000
#define notRendered mint

typedef enum { 
    rt_root,    /*-- only first ray is of this type*/
    rt_spec,    /*-- reflected ray*/
    rt_frac     /*-- transmitted ray*/
} NODE_TYPE;

typedef struct {
    int reflect;
    int refract;
    int next;
    NODE_TYPE type;
    int objPtr;
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
    _colour colour;
} NODE;

NODE cleanTree[maxNodes];

NODE *tree = cleanTree;
int freeNode;

int claim (int type ) {
    int nodePtr = freeNode;
    tree[nodePtr].reflect = nil;
    tree[nodePtr].refract = nil;
    tree[nodePtr].next = nil;
    tree[nodePtr].objPtr = nil;
    tree[nodePtr].type = type;
    freeNode++;
    return nodePtr;
}

int nrays=0;
#define minSect 0.001f

void sphereSect (NODE *node, object *sphere) {
    float xoff, yoff, zoff,
           xcomp, ycomp, zcomp,
           minusb, rootb2m4ac,
           b2, c;
    xoff = node->startx - sphere->u.sphere.x;
    yoff = node->starty - sphere->u.sphere.y;
    zoff = node->startz - sphere->u.sphere.z;

    xcomp = xoff * node->dx;
    ycomp = yoff * node->dy;
    zcomp = zoff * node->dz;

    minusb = -(xcomp + (ycomp + zcomp));
    b2     = minusb * minusb;

    xcomp = xoff * xoff;
    ycomp = yoff * yoff;
    zcomp = zoff * zoff;

    c = (xcomp + (ycomp + zcomp)) - (sphere->u.sphere.rad * sphere->u.sphere.rad);
    if (b2 > c) {
        float t1, t2;
        int proceed, t2ok, t1ok;
        rootb2m4ac = sqrtf (b2 - c );
        t1 = minusb - rootb2m4ac;
        t2 = minusb + rootb2m4ac;
        if (t1 > minSect) {
            t1ok = TRUE;
        } else {
            t1ok = FALSE;
        }
        if (t2 > minSect) {
            t2ok = TRUE;
        } else {
            t2ok = FALSE;
        }
        if (t1ok && t2ok) {
            proceed = TRUE;
            if (t2 > t1) {
                node->t = t1;
            } else {
                node->t = t2;
            }
        } else if (t1ok) {
            node->t = t1;
            proceed = TRUE;
        } else if (t2ok) {
            node->t = t2;
            proceed = TRUE;
        } else {
            proceed = FALSE;
        }
        if (proceed) {
            node->sectx = (node->dx * node->t) + node->startx;
            node->secty = (node->dy * node->t) + node->starty;
            node->sectz = (node->dz * node->t) + node->startz;
            node->normx = (node->sectx - sphere->u.sphere.x) / sphere->u.sphere.rad;
            node->normy = (node->secty - sphere->u.sphere.y) / sphere->u.sphere.rad;
            node->normz = (node->sectz - sphere->u.sphere.z) / sphere->u.sphere.rad;
        } else {
            node->t = 0;    /* -- no intersection */
        }
    } else {
        node->t = 0;
    }
}

void sceneSect ( int nodePtr, int shadowRay ) {
    if (nodePtr > maxNodes) {
        printf ("INVALID node %d\n", nodePtr);
        return;
    }
    NODE *node = &tree[nodePtr];
    int  ptr, objp, i;
    int proceed;
    NODE closest;
    nrays++;
    closest = *node;
    proceed = TRUE; /*-- a quick 'get out' clause for shadow checking*/
    objp   = nil;
    for (i=0; i < num_objects && proceed; i++) {
        object o = objects[i];
        switch (o.type) {
            case o_sphere:
                sphereSect ( node, &o );
            break;
            #if 0
            case o_ellipsoid:
                ellipsoidSect ( node, o );
            break;
            case o_cylinder:
                cylinderSect ( node, o );
            break;
            case o_cone:
                coneSect ( node, o );
            break;
            case o_plane:
                planeSect (node, o );
            break;
            case o_xyplane:
                xyPlaneSect (node, o );
            break;
            case o_yzplane:
                yzPlaneSect (node, o );
            break;
            case o_xzplane:
                xzPlaneSect (node, o );
            break;
            #endif
            default:
            /*
                writef (pixelsOut, "*N*CGarbage object %I in list at offset %I",
                    object [o.type], ptr, 0, 0 )
                STOP*/
            break;
        }
        if ((int)node->t == 0) {
            /* SKIP */
        } else if (shadowRay) {
            proceed = FALSE;
            node->objPtr = nil;
        } else if (objp == nil) {
            objp = i;   /*-- pointer to type slot in world model*/
            node->objPtr = objp;
            closest = *node;
        } else if (node->t < closest.t) {
            objp = i;   /*-- pointer to size slot in world model*/
            node->objPtr = objp;
            closest = *node;
        }
    }
    if (shadowRay) {

    } else {
        *node = closest;
    }
}

/* TODO probably wrong Occam->C */
float invert(float a) {
    int b = (int)a;
    //b = b ^ mint;
    b = -a;
    return (float)b;
}

void reflectRay ( int reflected, int incident, float *Vprime, float *Vvec, int *flip) {
    /*
    --
    -- this code assumes a ray normalized on entry. if it is not, you are in
    -- trouble
    --
    -- normalization is not maintained after a reflection, but from the
    -- geometry of the system ( see above) we can determine that
    -- the ratio of V' to V is the ratio of R' to R
    --
    -- now V is 1, R is 1, so to obtain R from R'
    -- R = (R' * V) / V'
    -- or R = R'/V'
    --
    -- so renormalization now costs 3 divides, not 3 muls, 2 adds and 1 root
    --
    */
    NODE *node = &tree[incident];
    NODE *spec = &tree[reflected];
    float twoN;
    float VN;
    VN = dotProduct (&node->dx,&node->normx);
    if (VN > 0.0) {
        node->normx = invert(node->normx);
        node->normy = invert(node->normy);
        node->normz = invert(node->normz);
        *Vprime = 1.0 / VN;
        *flip   = TRUE;
    } else {
        *Vprime = -1.0 / VN;
        *flip   = FALSE;
    }
    /*--
    --  V'/V = dxV' / dxV
    --                  _ _
    --  V = 1, V' = -1/(N.V)
    --
    --  so dxV' = dxV*Vprime
    --
    --  so Rx = Nx + Nx + dxV*Vprime
    --     Ry = Ny + Ny + dyV*Vprime
    --     Rz = Nz + Nz + dzV*Vprime
    --*/
    twoN     = node->normx * 2.0;
    Vvec[0]  = node->dx * *Vprime;
    spec->dx = Vvec[0]  + twoN;

    twoN     = node->normy * 2.0;
    Vvec[1]  = node->dy * *Vprime;
    spec->dx = Vvec[1]  + twoN;

    twoN     = node->normz * 2.0;
    Vvec[2]  = node->dz * *Vprime;
    spec->dx = Vvec[2]  + twoN;

    spec->dx = spec->dx / *Vprime;
    spec->dy = spec->dy / *Vprime;
    spec->dz = spec->dz / *Vprime;

    spec->startx = node->sectx + node->normx;
    spec->starty = node->secty + node->normy;
    spec->startz = node->sectz + node->normz;
}

void refractRay ( int refracted, int incident,
                    float *Vprime, float *Vvec,
                    int   flip, int *totalInternal ) {
    /*--   _       _   _     _
    --   P  = kf(N + V') - N
    --
    --                  2    2   _    _  2  -1/2
    --    where kf = (kn |V'| - |V' + N'| )
    --
    --    kn = index of refraction
    --               _ _
    --  NOTE that if V.N < 0 we are INSIDE the object - be careful about
    --  the refractive index*/

    NODE *node = &tree[incident];
    NODE *frac = &tree[refracted];

    object o = objects[node->objPtr];
    float kn = o.refix;
    float kn_Vprime, Vprime_plus_N;
    float t[3]; /* -- temporary vector */

    /*[3] REAL32 norm IS [ node FROM n.normx FOR 3 ] :
    [3] INT inorm RETYPES norm :
    */
    if (flip) {
        kn_Vprime = (*Vprime * *Vprime) / kn;
    } else {
        kn_Vprime = (*Vprime * *Vprime) * kn;
    }

    t[0] = node->normx + Vvec[0];
    t[1] = node->normy + Vvec[1];
    t[2] = node->normz + Vvec[2];

    Vprime_plus_N = (t[0] * t[0]) +
                    ((t[1] * t[1]) +
                    (t[2] * t[2]));

    if (Vprime_plus_N > kn_Vprime) {
        *totalInternal = TRUE;
    } else {
        float kf2, kf;
        *totalInternal = FALSE;

        kf2 = kn_Vprime - Vprime_plus_N;
        kf = sqrtf ( kf2 );

        t[0] = t[0] / kf;  /*-- get kf (N + V')*/
        t[1] = t[1] / kf;
        t[2] = t[2] / kf;

        frac->dx = t[0] - node->normx;
        frac->dy = t[1] - node->normy;
        frac->dz = t[2] - node->normz;
        normalize (&frac->dx, &kf);
        frac->startx = node->sectx - node->normx;
        frac->starty = node->secty - node->normy;
        frac->startz = node->sectz - node->normz;
    }
}

int createRay (int rootRay, float x, float y) {
    NODE *node = &tree[rootRay];
    float scrx, scry, hyp=0.0f;
    scrx = 512.0-x;
    scry = 512.0-y;
    node->startx = rundata.screenOrg[0] + ((scrx * rundata.screenX[0]) + (scry * rundata.screenY[0]));
    node->starty = rundata.screenOrg[1] + ((scrx * rundata.screenX[1]) + (scry * rundata.screenY[1]));
    node->startz = rundata.screenOrg[2] + ((scrx * rundata.screenX[2]) + (scry * rundata.screenY[2]));
    node->dx = rundata.pinhole[0] - node->startx;
    node->dy = rundata.pinhole[1] - node->starty;
    node->dz = rundata.pinhole[2] - node->startz;
    normalize (&node->dx, &hyp);
}

int head;

int evolveNode (int *intoNode, int *outofNode, int nodePtr) {
    int spawn;
    NODE *node;
    sceneSect (nodePtr, FALSE); /* -- NOT casting shadow rays */
    node = &tree[nodePtr];
    if (node->objPtr == nil) {
        spawn = 0;
    } else {
        int objPtr = node->objPtr;
        int attr = objects[objPtr].attr;
        float Vprime;
        float Vvec[3];
        int flect, signFlip, tIR;
        int spec;
        if (attr & a_spec) {
            spec = claim (rt_spec);
            node->reflect = spec;
            *intoNode = spec;
            reflectRay (spec, nodePtr, &Vprime, Vvec, &signFlip);
            flect = TRUE;
        } else {
            flect = FALSE;
        }
        if (flect && attr & a_frac) {
            int frac;
            frac = claim(rt_frac);
            refractRay (frac, nodePtr, &Vprime, Vvec, signFlip, &tIR);
            if (tIR) {
                *outofNode = spec;
                spawn = 1;
            } else {
                node->refract = frac;
                tree[spec].next = frac;
                spawn = 2;
                *outofNode = frac;
            }
        } else if (flect) {
            *outofNode = spec;
            spawn = 1;
        } else {
            spawn = 0;
        }
    }
    return spawn;
}

/* TODO really not sure about the 'C' conversion! */
int evolveTree (void) {
    int nodesAdded;
    int node, next=0, prev=0;
    node = head;
    next = node;    /* overwritten by evolveNode */
    nodesAdded = evolveNode (&head, &next, node);
    while (tree[node].next != nil) {
        int prev_temp=nil;
        prev = next;
        next = node;
        node = tree[node].next;
        nodesAdded += evolveNode (&prev_temp, &next, node);
        tree[prev].next = prev_temp;
    }
    tree[next].next = nil;
    return nodesAdded;
}

int buildShadeTree (Channel *out, float x, float y) {
    int nodes = 0;
    int depth = 1;
    int rootNode=0;
    int newNodes;
    freeNode = 0;
    if (x==190 && y==172) {
        printf ("problematic node begins!\n");
    }
    rootNode = claim (rt_root);
    createRay (rootNode,x,y);
    head = rootNode;
    tree[rootNode].next = nil;
    newNodes = evolveTree ();
    while (depth < maxDepth && newNodes != 0) {
        nodes = nodes + newNodes;
        depth = depth + 1;
        newNodes = evolveTree ();
    }
    return rootNode;
}

void mixChildren ( int nodePtr ) {
    /*--
    -- Total intensity at node is sum of ambient, diffuse, glossy, specular and
    -- transmitted intensities.
    --
    --
    --                    j=ls _ _
    -- I = Ia + Ig + kd *   { (N.Lj)
    --                    j=1
    --
    -- Here we attenuate specular and transmitted components
    -- from the children, and add into the parent node.
    --*/
    NODE *node = &tree[nodePtr];
    object o = objects[node->objPtr];
    _colour black = {0.0, 0.0, 0.0};
    _colour xmit;
    _colour spec;
    int specPtr = node->reflect;
    int fracPtr = node->refract;
    if (specPtr == nil) {
        spec = black;
    } else {
        /*-- specular contribution is independent of colour of surface
        -- but here we should attenuate according to 't' ( distance
        -- ray has travelled)*/
        spec = tree[specPtr].colour;
        spec.r = spec.r * o.ks;
        spec.g = spec.g * o.ks;
        spec.b = spec.b * o.ks;
    }
    if (fracPtr == nil) {
        xmit = black;
    } else {
        /* -- colour the transmitted light
           -- but here we should attenuate according to 't' of ray*/
        xmit = tree[fracPtr].colour;
        xmit.r = xmit.r * o.xmitR;
        xmit.g = xmit.g * o.xmitG;
        xmit.b = xmit.b * o.xmitB;
    }
    node->colour.r = node->colour.r + (spec.r + xmit.r);
    node->colour.g = node->colour.g + (spec.g + xmit.g);
    node->colour.b = node->colour.g + (spec.b + xmit.b);
}

#define a11 0
#define a12 1
#define a13 2
#define a21 3
#define a22 4
#define a23 5
#define a31 6
#define a32 7
#define a33 8

#define pxx 0
#define pyy 1
#define pzz 2

void TransLocal(float *matrix/*9*/,float *point/*3*/) {
    float newpoint[3];
    newpoint[pxx] = (( matrix[a11] * point[pxx] ) +
                    (( matrix[a12] * point[pyy] ) +
                        ( matrix[a13] * point[pzz] )));
    newpoint[pyy] = (( matrix[a21] * point[pxx] ) +
                    (( matrix[a22] * point[pyy] ) +
                        ( matrix[a23] * point[pzz] )));
    newpoint[pzz] = (( matrix[a31] * point[pxx] ) +
                    (( matrix[a32] * point[pyy] ) +
                        ( matrix[a33] * point[pzz] )));
    point[pxx] = newpoint[pxx];
    point[pyy] = newpoint[pyy];
    point[pzz] = newpoint[pzz];
}

void TransGlobal(float *matrix/*9*/,float *point/*3*/ ) {
    float invers[9];
    invers[a11]=matrix[a11];
    invers[a12]=matrix[a21];
    invers[a13]=matrix[a31];
    invers[a21]=matrix[a12];
    invers[a22]=matrix[a22];
    invers[a23]=matrix[a32];
    invers[a31]=matrix[a13];
    invers[a32]=matrix[a23];
    invers[a33]=matrix[a33];
    TransLocal(invers,point);
}

void shadeNode ( int nodePtr ) {
/*
    --
    -- Total intensity at node is sum of ambient, diffuse, glossy,
    -- specular and transmitted intensities.
    --
    --                            j=ls
    --                             _  _ _
    -- I = (Ia * kd) + Ig + kd *   > (N.Lj)
    --                             -
    --                            j=1
    --
    -- Here we compute ambient diffuse and glossy terms, specular
    -- and transmitted components are added at mix time.
    --
    -- Ig is given by Phong
    --
    --      j=ls
    --       _  _ _   n
    -- Ig =  > (N.Lj')
    --       -
    --      j=1
    --
    --
    --
*/
    NODE *node = &tree[nodePtr];
    int objPtr = node->objPtr;
    _colour ambient = rundata.ambient;
    _colour *colour = &node->colour;
    _colour black = {0.0,0.0,0.0};
    if (objPtr == nil) {
        *colour = ambient;
    } else {
        object o = objects[objPtr];
        float spec = o.kg;
        int attr = o.attr;

        int shadow, phong;  /*-- ptr into tree for shadow ray, pseudo light*/
        int lightPtr;
        NODE *shadowNode;
        NODE *phongNode;
        if ((attr & a_spec) == 0) {
            colour->r = ambient.r * o.kdR;
            colour->g = ambient.g * o.kdG;
            colour->b = ambient.b * o.kdB;
        } else {
            *colour = black;
        }
        shadow = claim(rt_root);
        phong = claim(rt_root);

        shadowNode = &tree[shadow];
        phongNode = &tree[phong];

        lightPtr = 0;
        shadowNode->startx = node->sectx + node->normx;
        shadowNode->starty = node->secty + node->normy;
        shadowNode->startz = node->sectz + node->normz;
        for (lightPtr=0; lightPtr < num_lights; lightPtr++) {
            light l = lights[lightPtr];
            /*-- set up shadow ray direction cosines */
            shadowNode->dx = l.dx;
            shadowNode->dy = l.dy;
            shadowNode->dz = l.dz;
            sceneSect ( shadow, TRUE );
            if (shadowNode->t != 0.0) {

            } else {
                float lambert;
                int iLambert;
                // node->normx when nodePtr!=0 = -inf => lambert = nan
                lambert = dotProduct (&l.dx, &node->normx);
                iLambert = (int)lambert;
                // ignoring this -ve check does create a multi-colour shading!
                if ((iLambert & 0x80000000) != 0) { /*-- -ve !*/
                } else {
                    colour->r = colour->r + (lambert * (o.kdR * l.ir));
                    colour->g = colour->g + (lambert * (o.kdG * l.ig));
                    colour->b = colour->b + (lambert * (o.kdB * l.ib));
                }
                /*--
                -- the phong shader is a mite messy at the moment, with lots of
                -- sign inversions all over the place. I'm not sure they can be avoided
                --*/
                if (attr & a_spec != 0) {
                    float cosPhong;
                    int iCosPhong;
                    float Vprime;   /*-- to keep reflect ray happy */
                    float Vvec[3];
                    int signFlip;
                    float flipNode[3];

                    shadowNode->normx = node->normx;
                    shadowNode->normy = node->normy;
                    shadowNode->normz = node->normz;
                    shadowNode->sectx = node->sectx;
                    shadowNode->secty = node->secty;
                    shadowNode->sectz = node->sectz;

                    /*--
                    --  a nasty sign inversion due to the light direction being optimized
                    --  for shadow spotting
                    --*/
                    /* TODO check this! */
                    shadowNode->dx = invert(shadowNode->dx);
                    shadowNode->dy = invert(shadowNode->dy);
                    shadowNode->dz = invert(shadowNode->dz);
                    reflectRay ( phong, shadow, &Vprime,
                                    Vvec, &signFlip );
                    /*--
                    --  again, a nasty sign inversion is required here - see diagram
                    -- newmann / sproull, p. 391
                    --*/
                    flipNode[0] = invert(node->dx);
                    flipNode[1] = invert(node->dy);
                    flipNode[2] = invert(node->dz);
                    cosPhong = dotProduct (flipNode, &phongNode->dx);
                    /*TODO possibly wrong Occam->C though the simple invert seems to prouce better results
                    iCosPhong = (int)cosPhong;
                    if (iCosPhong & mint != 0) {
                        iCosPhong = invert(iCosPhong);
                    }
                    cosPhong = (float)iCosPhong;*/
                    if (cosPhong<0) {
                        // TODO not sure why the Occam version is so complex!
                        cosPhong = -cosPhong;
                    } else {
                        cosPhong = (cosPhong * cosPhong); /*-- power := 2*/
                        cosPhong = (cosPhong * cosPhong); /*-- power := 4*/
                        cosPhong = (cosPhong * cosPhong); /*-- power := 8*/
                        colour->r = colour->r + (spec * (cosPhong * l.ir));
                        colour->g = colour->g + (spec * (cosPhong * l.ig));
                        colour->b = colour->b + (spec * (cosPhong * l.ib));
                    }
                } else {
                }
            }
        }
    }
}

typedef enum {
    a_stop,
    a_refract,
    a_mix,
    a_reflect
} SHADE_ACTION;

typedef struct {
    int np;
    SHADE_ACTION action;
} SHADE_STACK_ENTRY;

void shade ( int rootNode ) {
/*
    --  Action
    --  ------
    --
    --  We enter proc shade with a pointer into the root node of the tree,
    --  and exit with 3 10-bit integer values in the root node's r g b fields
    --
    --  This proc performs bottom-up shading of the ray intersection tree
    --
    --  Its action is equivalent to
    --
    --  PROC shade ( VAL INT node )
    --    SEQ
    --      IF
    --        tree [ node + n.reflect] <> nil
    --          shade ( tree [ node + n.reflect])
    --        TRUE
    --          SKIP
    --      IF
    --        tree [ node + n.refract] <> nil
    --          shade ( tree [ node + n.refract])
    --        TRUE
    --          SKIP
    --      shadeNode ( node)
    --      mixNode   ( node)
    --  :
    --
    --  where shadeNode () shades this intersection according to lighting
    --        conditions and surface properties
    --
    --  and   mixNode () mixes together the node + its children according to the
    --        surface transmission / reflectance properties
    --
    */
    SHADE_STACK_ENTRY stack[(maxDepth + 2)];
    int sp;

    int nodePtr, action;
    sp = 0;
    /* -- pre load stack with 'terminate' action */
    stack[sp].np = 0;
    stack[sp].action = a_stop;  
    sp++;
    nodePtr = rootNode;
    action  = a_reflect;
    while (action != a_stop) {
        NODE node;
        int spec;
        int frac;
        node = tree[nodePtr];
        spec = node.reflect;
        frac = node.refract;
        if (action == a_reflect) {
            if (spec == nil) {
                /* -- no reflected ray, shade refracted ray */
                action = a_refract;
            } else if (frac == nil) {
                stack[sp].action = a_mix;
                stack[sp].np = nodePtr;
                sp++;
                nodePtr = spec;
            } else {
                /* -- direction to go on return */
                stack[sp].action = a_refract;
                stack[sp].np = nodePtr;
                sp++;
                nodePtr = spec;
            }
        }
        else if (frac == nil) {
            shadeNode ( nodePtr );      /* -- shade leaf node */
            sp--;
            action = stack[sp].action;
            nodePtr = stack[sp].np;
            while (action == a_mix) {
                /* -- all these nodes have had their children shaded, so we can shade nd mix them */
                shadeNode   ( nodePtr );
                mixChildren ( nodePtr );
                sp--;
                action = stack[sp].action;
                nodePtr = stack[sp].np;
            }
        } else {
            stack[sp].action = a_mix;
            stack[sp].np = nodePtr;
            sp++;
            nodePtr = frac;
            action  = a_reflect;
        }
    }
    {
        NODE *root = &tree[rootNode];

        float red  = root->colour.r;
        float green = root->colour.g;
        float blue = root->colour.b;

        int ired = (int)root->colour.r;
        int igreen = (int)root->colour.g;
        int iblue = (int)root->colour.b;

        float maxC = 1022.99; /*-- just under 10 bits*/
        float maxP, fudge;
        int desaturate;
        desaturate = TRUE;
        if (red > green) {
            maxP = red;
        } else {
            maxP = green;
        }
        if (maxP > blue) {
            if (maxP > maxC) {
                fudge = maxC / maxP;
            } else {
                desaturate = FALSE;
            }
        } else if (blue > maxC) {
            fudge = maxC / blue;
        } else {
            desaturate = FALSE;
        }
        if (desaturate) {
            ired   = (int)(red   * fudge);
            igreen = (int)(green * fudge);
            iblue  = (int)(blue  * fudge);
        } else {
            ired   = (int)red;
            igreen = (int)green;
            iblue  = (int)blue;
        }
        root->colour.r = ired;
        root->colour.g = igreen;
        root->colour.b = iblue;
    }
}

int samples[GRID_SIZE][GRID_SIZE];

int pointSample (Channel *out, int patchx, int patchy, int x, int y ) {
    int colour = samples[y][x];
    static int first=0;
    if (first==0) {
        Debug (out, "GRID_SIZE", GRID_SIZE,GRID_SIZE);
        Debug (out, "sizeof(samples)", sizeof(samples),sizeof(samples));
        first=1;
    }
    if (colour == notRendered) {
        int tx, ty;
        float wx, wy;
        int treep=0;
        NODE node;
        tx = (patchx << maxDescend) + x;
        ty = (patchy << maxDescend) + y;

        wx = (float)tx / (float)descendPower;
        wy = (float)ty / (float)descendPower;
        treep = buildShadeTree (out, wx, wy);
        shade (treep);
        node = tree[treep];
        colour = (int)node.colour.r | (int)node.colour.g << colourBits | (int)node.colour.b << (colourBits + colourBits);
    } else {
        Debug (out, "point already rendered!", x, y);
    }
    return colour;
}

typedef struct {
    int action; /* a_render, a_shade or a_stop */
    int hop;    /* if a_render */
    int y;      /* if a_render */
    int x;      /* if a_render */
} STACK_ENTRY;

void renderPixels ( int patchx, int patchy,
                    int x0, int y0,
                    int *colour,
                    int renderingMode,
                    Channel *out,
                    int debug) {
    STACK_ENTRY stack[maxDescend * 4];   /* 4 * (render, x, y, hop); shade */
    int colstack[(maxDescend + 1) * 4];
    int sp, cp;
    int x, y, hop;
    int action;
    const int a_render=0;
    const int a_shade=1;
    const int a_stop=2;

    if (x0==0 && y0==0) {
//        Debug (out, "start renderPixels", patchx+x0, patchy+y0);
    }
    if (renderingMode == m_adaptive) {
        cp        = 0;                      /*-- empty colour stack */
        sp = 0;
        stack[sp].action = a_stop;
        sp++;                               /*-- pre load stack with render x y hop; stop*/
        stack[sp].action = a_render;
        stack[sp].hop = descendPower;       /*-- set grid hop value to gross pixel level*/
        stack[sp].y = y0 << maxDescend;
        stack[sp].x = x0 << maxDescend;     /*-- locations within this patch*/
        do {
            action = stack[sp].action;
            if (action == a_render) {
                int a, b, c, d, rRange, gRange, bRange;
                int sg = colourBits;
                int sb = colourBits + colourBits;
                x   = stack[sp].x;
                y   = stack[sp].y;
                hop = stack[sp].hop;
                // a,b,c,d will be BGR samples (10bpp)
                a = pointSample ( out, patchx, patchy, x,       y      );
                b = pointSample ( out, patchx, patchy, x + hop, y      );
                c = pointSample ( out, patchx, patchy, x,       y + hop);
                d = pointSample ( out, patchx, patchy, x + hop, y + hop);
                {
                    int blue = (a>>22)&0xff;
                    int green = (a>>12)&0xff;
                    int red = (a>>2)&0xff;
                }
                rRange = findRange (a & rMask, b & rMask,
                                    c & rMask, d & rMask);

                gRange = findRange ( (a & gMask) >> sg, (b & gMask) >> sg,
                                    (c & gMask) >> sg, (d & gMask) >> sg);

                bRange = findRange ( a >> sb, b >> sb,
                                    c >> sb, d >> sb);
                //TODO work out why gets stuck in shade/render loop. with FALSE here it proceeds and renders something!
                if (FALSE && hop != 1 &&
                    (rRange > threshold ||
                     gRange > threshold ||
                     bRange > threshold)) {
                        hop = hop >> 1;
                        sp++;
                        Debug (out, "push shade",x,y);
                        stack[sp].action = a_shade;
                        sp++;
                        stack[sp].hop = hop;
                        stack[sp].y = y;
                        stack[sp].x = x;
                        stack[sp].action = a_render;
                        sp++;
                        stack[sp].hop = hop;
                        stack[sp].y = y;
                        stack[sp].x = x+hop;
                        stack[sp].action = a_render;
                        sp++;
                        stack[sp].hop = hop;
                        stack[sp].y = y+hop;
                        stack[sp].x = x;
                        stack[sp].action = a_render;
                        sp++;
                        stack[sp].hop = hop;
                        stack[sp].y = y+hop;
                        stack[sp].x = x+hop;
                        stack[sp].action = a_render;
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
                    colstack[cp] = B | G | R;
                    cp++;
                }
            } else if (action == a_shade) {
                int a, b, c, d, R, G, B, m;
                Debug (out, "a_shade",x,y);
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
                colstack[0] = B | G | R;
                cp-=3;
            } else if (action == a_stop) {
            } else {
                Debug (out, "*E* renderPixels bad action",action,action);
            }
            sp--;
        } while (action != a_stop);
        *colour = colstack[cp-1];
    } else if (renderingMode == m_dumb) {
        int   tx, ty;
        float wx, wy;
        int treep=0;
        NODE node;
        tx = ((patchx + x0) << maxDescend);
        ty = ((patchy + y0) << maxDescend);

        wx = (float)tx / (float)descendPower;
        wy = (float)ty / (float)descendPower;

        treep = buildShadeTree ( out, wx, wy );
        shade ( treep );
        node = tree[treep];
        *colour = (int)node.colour.r | (int)node.colour.g << colourBits | (int)node.colour.b << (colourBits + colourBits);
    } else if (renderingMode == m_test) {
        *colour = patchx*patchy*640;
        #ifdef NATIVE
        *colour |= 0xFF000000;  // ensure Alpha is set to opaque
        #endif
    }
}

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
                ChanIn(job_in,(char *)&o, (int)sizeof(o));
                *po = o;
                po++;
                num_objects++;
            }
            break;
            case c_light:
            {
                light l;
                ChanIn(job_in,(char *)&l, (int)sizeof(l));
                *pl = l;
                pl++;
                num_lights++;
            }
            break;
            case c_runData:
            {
                ChanIn(job_in,(char *)&rundata, (int)sizeof(rundata));
            }
            break;
            case c_start:
                loading_scene = 0;
            break;
            default:
                Debug (rsl_out, "*E* unknown command", type, 0);
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

            ChanIn(job_in,(char *)&r,(int)sizeof(r));
            /* initWORDvec ( rawSamples, notRendered, gridSize * gridSize ) */
            for (y=0; y < GRID_SIZE; y++) {
                for (x=0; x < GRID_SIZE; x++) {
                    samples[y][x] = notRendered;
                }
            }
            {
                static int done=0;
                if (done==0) {
                    Debug (rsl_out, "c_render num_objects", num_objects, 0);
                    Debug (rsl_out, "c_render num_lights", num_lights, 0);
                    Debug (rsl_out, "c_render r.w r.h", r.w, r.h);
                    done = 1;
                }
            }
            pbuf = buf;
            for (y = 0; y < r.h; y++) {
                for (x = 0; x < r.w; x++)
                {
#if 1
                    renderPixels (r.x, r.y, x, y, pbuf, rundata.renderingMode, rsl_out, r.x==0 && r.y==0 && x==0 && y==0);
#else
                    renderPixels (r.x, r.y, x, y, pbuf, m_dumb, rsl_out, r.x==0 && r.y==0 && x==0 && y==0);
/*                    renderPixels (r.x, r.y, x, y, pbuf, m_test, rsl_out, r.x==0 && r.y==0 && x==0 && y==0);*/
#endif
                    pbuf++;
                }
            }
            p.x = r.x;
            p.y = r.y;
            p.worker = 0;   /* TODO */
            p.patchWidth = r.w;
            p.patchHeight = r.h;
            ChanOutInt(rsl_out,c_patch);
            ChanOut(rsl_out,(char *)&p,(int)sizeof(p));
            ChanOut(rsl_out, buf, p.patchWidth*p.patchHeight*4);
        } else {
            Debug (rsl_out, "*E* unknown command", type, 0);
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
                ChanIn(buf_in,(char *)&o, (int)sizeof(o));
                ChanOutInt(buf_out,type);
                ChanOut(buf_out,&o,(int)sizeof(o));
            }
            break;
            case c_light:
            {
                light l;
                ChanIn(buf_in,(char *)&l, (int)sizeof(l));
                ChanOutInt(buf_out,type);
                ChanOut(buf_out,&l,(int)sizeof(l));
            }
            break;
            case c_runData:
            {
                _rundata r;
                ChanIn(buf_in,(char *)&r, (int)sizeof(r));
                ChanOutInt(buf_out,type);
                ChanOut(buf_out,&r,(int)sizeof(r));
            }
            break;
            case c_start:
                ChanOutInt(buf_out,type);
                loading_scene = 0;
            break;
            default:
                /*while(1) {lon();}*/
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
            ChanIn(buf_in,(char *)&r, (int)sizeof(r));
            ChanOutInt(buf_out, type);
            ChanOut(buf_out, (char *)&r, (int)sizeof(r));
        } else {
            /*while(1) {lon();}*/
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
                ChanIn(sel_in,(char *)&o, (int)sizeof(o));
                /* send the object to each worker */
                for (i=0; i < 4; i++) {
                    if (dn_out[i]!=(void *)0) {
                        ChanOutInt(dn_out[i],type);
                        ChanOut(dn_out[i],&o,(int)sizeof(o));
                    }
                }
            }
            break;
            case c_light:
            {
                light l;
                int i;
                ChanIn(sel_in,(char *)&l, (int)sizeof(l));
                /* send the object to each worker */
                for (i=0; i < 4; i++) {
                    if (dn_out[i]!=(void *)0) {
                        ChanOutInt(dn_out[i],type);
                        ChanOut(dn_out[i],&l,(int)sizeof(l));
                    }
                }
            }
            break;
            case c_runData:
            {
                _rundata r;
                int i;
                ChanIn(sel_in,(char *)&r, (int)sizeof(r));
                /* send the object to each worker */
                for (i=0; i < 4; i++) {
                    if (dn_out[i]!=(void *)0) {
                        ChanOutInt(dn_out[i],type);
                        ChanOut(dn_out[i],&r,(int)sizeof(r));
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
                /*while(1) {lon();}*/
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
                ChanIn(sel_in,(char *)&r,(int)sizeof(r));
                /* 2. wait for any worker to become ready */
                i = ProcAltList(req_in);
                ChanInInt(req_in[i]);       /* discard the 0 */
                /* 3. send the job to the worker */
                ChanOutInt(dn_out[i],type);
                ChanOut(dn_out[i],(char *)&r,(int)sizeof(r));
            }
            break;
            default:
                /*while(1) {lon();}*/
            break;
        }
    }
}

/* This runs on the root node only. Split full image into MAXPIX (or less) sized jobs */
void feed(Channel *host_in, Channel *host_out, Channel *fd_out, void *fxp)
{
    loop
    {
        int type;
        type = ChanInInt(host_in);
        switch (type) {
            case c_object:
            {
                object o;
                ChanIn(host_in,(char *)&o, (int)sizeof(o));
                /* pass downstream */
                ChanOutInt (fd_out, type);
                ChanOut(fd_out,(char *)&o,(int)sizeof(o));
                /* ack to host */
                ChanOutInt(host_out,c_object_ack);
            }
            break;
            case c_light:
            {
                light l;
                ChanIn(host_in,(char *)&l, (int)sizeof(l));
                ChanOutInt (fd_out, type);
                ChanOut(fd_out,(char *)&l,(int)sizeof(l));
                ChanOutInt(host_out,c_light_ack);
            }
            break;
            case c_runData:
            {
                _rundata r;
                ChanIn(host_in,(char *)&r, (int)sizeof(r));
                ChanOutInt (fd_out, type);
                ChanOut(fd_out,(char *)&r,(int)sizeof(r));
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
                            ChanOut(fd_out,(char *)&r,(int)sizeof(r));
                        }
                    }
                }
                /* inform host that work is finished */
                ChanOutInt(host_out,c_done);
            }
            break;
            default:
                ChanOutInt(host_out,c_message2);
                ChanOutInt(host_out,type);
                ChanOutInt(host_out, 0);
                ChanOutInt(host_out,12);
                Debug(host_out,"FEED BAD CMD",0,0);
            break;
        }
    }
}
