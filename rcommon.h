
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

#define m_adaptive   0
#define m_stochastic 1
#define m_dumb       2
#define m_test       3

#define n_reflect 0        /* point to children */
#define n_refract 1
#define n_next    2        /* maintain linked list of leaf nodes */
#define n_type    3        /* reflected, refracted or primary ray */

/* Valid ray types for n_type slot */

#define n_t       11       /* distance parameter 't' of intersection */

#define n_startx  12       /* ray origin */
#define n_starty  13
#define n_startz  14

#define n_dx      15       /* ray direction */
#define n_dy      16
#define n_dz      17

#define n_red   18
#define n_green 19
#define n_blue  20

#define nodeSize n_blue + 1

#define maxDepth 6
#define maxNodes 4 + (2 << maxDepth)  /* traverse tree 5 deep max, */
                                       /* PLUS 4 for good measure */
#define treeSize  nodeSize * maxNodes /* shadow generation */

#define o_sphere    0
#define o_xyplane   1
#define o_xzplane   2
#define o_yzplane   3
#define o_plane     4
#define o_cone       5
#define o_cylinder   6
#define o_ellipsoid  7
#define o_quadric    8 /* general quadric, hell to compute */

#define x_axis      0
#define y_axis      1
#define z_axis      2

/* the attribute word contains gross attributes of the object as */
/* bits. The relevant words in the object descriptor expand upon these */

#define a_spec     0x01  /* use lambert / use phong and reflect */
#define a_frac     0x02  /* refract on surface */
#define a_bound0   0x04  /* bounded in 1 dimension */
#define a_bound1   0x08  /* bounded in 2 dimension */
#define a_tex      0x10  /* texture map ? */
#define a_bump     0x20  /* bump map    ? */

typedef struct {
    int type;
    int attr;
    float kdR;
    float kdG;
    float kdB;
    float ks;
    float kg;
    float xmitR;
    float xmitG;
    float xmitB;
    float power;
    float refix;
    union {
        struct {

        }
    }
} object;

#if 0
#define o_type      0 /* type field of all objects */
#define o_attr      1 /* attribute field of all object */

#define o_kdR       2  /* coeffs of diffuse reflection R G B */
#define o_kdG       3
#define o_kdB       4

#define o_ks        5  /* coeff of specular reflection ( for reflections) */

#define o_kg        6  /* coeff of gloss for phong shading */
#define o_xmitR     7  /* coeffs of transmission R G B */
#define o_xmitG     8
#define o_xmitB     9
#endif

#define o_power    10  /* power for phong shading */
#define o_refix    11  /* refractive index (relative) */

#define s_rad       first + 0
#define s_x         first + 1
#define s_y         first + 2
#define s_z         first + 3

#define s_map       first + 4  /* which map ( texture or bump) */
#define s_uoffs     first + 5  /* offsets for u and v in map */
#define s_voffs     first + 6
#define s_size      first + 7

#define co_x     first + 0       /* center of the circle */
#define co_y     first + 1       /*  at the beginnig of the cone */
#define co_z     first + 2
#define co_a11   first + 3       /* transformation matrix of the cone */
#define co_a12   first + 4       /* from gobal to local coord_ */
#define co_a13   first + 5
#define co_a21   first + 6
#define co_a22   first + 7
#define co_a23   first + 8
#define co_a31   first + 9
#define co_a32   first + 10
#define co_a33   first + 11
#define co_rada  first + 12      /* size of the circle at the beginning */
#define co_radb  first + 13      /* size of the circle at the end */
#define co_len   first + 14      /* length of the cone on new z axis */
#define co_size  first + 15

#define e_x     first + 0       /* center of the ellipsoid */
#define e_y     first + 1
#define e_z     first + 2
#define e_a11   first + 3       /* transformation matrix of the ellipsoid */
#define e_a12   first + 4       /* from gobal to local coord_ */
#define e_a13   first + 5
#define e_a21   first + 6
#define e_a22   first + 7
#define e_a23   first + 8
#define e_a31   first + 9
#define e_a32   first + 10
#define e_a33   first + 11
#define e_radx  first + 12      /* size on x axis ( of the new system ) */
#define e_rady  first + 13      /* size on y axis ( of the new system ) */
#define e_radz  first + 14      /* size on z axis ( of the new system ) */
#define e_size  first + 15

#define c_x     first + 0       /* center of the circle */
#define c_y     first + 1       /*  at the beginnig of the cylinder */
#define c_z     first + 2
#define c_a11   first + 3       /* transformation matrix of the cylinder */
#define c_a12   first + 4       /* from gobal to local coord_ */
#define c_a13   first + 5
#define c_a21   first + 6
#define c_a22   first + 7
#define c_a23   first + 8
#define c_a31   first + 9
#define c_a32   first + 10
#define c_a33   first + 11
#define c_rad   first + 12      /* size of the circle */
#define c_len   first + 13      /* length of the cylinder */
#define c_size  first + 14

#define p_x        first + 1 
#define p_y        first + 2 
#define p_z        first + 3 
#define p_ux       first + 4 
#define p_uy       first + 5 
#define p_uz       first + 6 
#define p_vx       first + 7 
#define p_vy       first + 8 
#define p_vz       first + 9 
#define p_wx       first + 10
#define p_wy       first + 11
#define p_wz       first + 12
#define p_sizeu    first + 13
#define p_sizev    first + 14

#define p_map      first + 15  /* which map ( texture or bump) */
#define p_uoffs    first + 16  /* offsets for u and v in map */
#define p_voffs    first + 17
#define p_size     first + 18

#define pxy_x        first + 0 /* origin x of the plane */
#define pxy_y        first + 1 /* origin y of the plane */
#define pxy_z        first + 2 /* origin z of the plane */
#define pxy_sizex    first + 3 /* bounding on x axis */
#define pxy_sizey    first + 4 /* bounding on y axis */
#define pxy_map      first + 5  /* which map ( texture or bump) */
#define pxy_uoffs    first + 6  /* offsets for u and v in map */
#define pxy_voffs    first + 7
#define pxy_size     first + 8

#define pxz_x        first + 0 /* origin x of the plane */
#define pxz_y        first + 1 /* origin y of the plane */
#define pxz_z        first + 2 /* origin z of the plane */
#define pxz_sizex    first + 3 /* bounding on x axis */
#define pxz_sizez    first + 4 /* bounding on z axis */
#define pxz_map      first + 5  /* which map ( texture or bump) */
#define pxz_uoffs    first + 6  /* offsets for u and v in map */
#define pxz_voffs    first + 7
#define pxz_size     first + 8

#define pyz_x        first + 0 /* origin x of the plane */
#define pyz_y        first + 1 /* origin y of the plane */
#define pyz_z        first + 2 /* origin z of the plane */
#define pyz_sizey    first + 3 /* bounding on y axis */
#define pyz_sizez    first + 4 /* bounding on z axis */
#define pyz_map      first + 5  /* which map ( texture or bump) */
#define pyz_uoffs    first + 6  /* offsets for u and v in map */
#define pyz_voffs    first + 7
#define pyz_size     first + 8

#define l_ir   0   /* light intensity fields  R G B */
#define l_ig   1
#define l_ib   2
#define l_dx   3   /* light direction cosines x y z */
#define l_dy   4   /* this is a quick'n'tacky shading sheme */
#define l_dz   5

#define l_size 6

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
#define PRBSIZE 12 

/* RSLCOM
0 len
1 x
2 y
3 pixels[len-3]
*/
#define RSLCOM_BUFSIZE 3+MAXPIX_WORDS  /* 32 bit words */

