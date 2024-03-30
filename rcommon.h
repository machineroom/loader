
/* Definitions shared between the Transputer and host code */

#define JOBCOM 0L
#define PRBCOM 1L
#define DATCOM 2L
#define RSLCOM 3L
#define FLHCOM 4L

/*-------------------------- COMMANDS PROTOCOL -----------------------------*/

#define c_invalid  -1
#define c_stop      0 
#define c_render    1   /* render; x0; y0 */
#define c_object    2   /* object; size; [ size ] */
#define c_light     3   /* light;  size; [ size ] */
#define c_patch     4   /* patch;  x; y; patchSize; worker; [ patchSize][patchSize] */
#define c_runData   5   /* ambient light, rendering mode, screen pos etc. */
#define c_message   6   /* message; size; [ words ] */
#define c_map       7 
#define c_mapAck    8   /* count */
#define c_message2  9   /* message; p1; p2; size; [ words ] */


#define MAXPIX  32 /* keep it a multiple of 4 please! TODO why can't this be <32? */
#define MAXPIX_WORDS MAXPIX/4

/* DATCOM {         [buf index]
    long com;       0 =DATCOM
    long fxp;       1
    long maxcnt;    2
    double lo_r;    3
    double lo_i;    5
    double gapx;    7
    double gapy;    9
} */
#define DATCOMSIZE 11

/* JOBCOM {         [buf index]
    long com;       0 =JOBCOM
    long x;         1
    long y;         2
    long width;     3
} */
#define JOBCOMSIZE 4

/* PRBCOM {         [buf index]
    long com;       0 =PRBCOM
    long width;     1
    long height;    2
    long maxcnt;    3
    double lo_r;    4
    double lo_i;    6
    double gapx;    8
    double gapy;    10
} */
#define PRBSIZE 12  /* Must be larger than DATCOM and JOBCOM. Sheesh :( */

/* RSLCOM
0 len
1 x
2 y
3 pixels[len-3]
*/
/* RSLCOM_BUFSIZE needs to be the largest buffer since it's reused in other places for smaller buffers. Sigh :( */
#define RSLCOM_BUFSIZE 3+MAXPIX_WORDS  /* 32 bit words */

