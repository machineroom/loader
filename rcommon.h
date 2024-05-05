
/* Definitions shared between the Transputer and host code */

#define BLOCK_WIDTH 8
#define BLOCK_HEIGHT 8

#define MAXPIX  (BLOCK_WIDTH*BLOCK_HEIGHT)

#define MAX_OBJECTS 12
#define MAX_LIGHTS 5

/*-------------------------- COMMANDS PROTOCOL -----------------------------*/

#define c_invalid  -1
#define c_stop      0 
#define c_render    1   /* render; */
#define c_object    2   /* object; */
#define c_light     3   /* light; */
#define c_patch     4   /* patch;  x; y; patchSize; worker; [ patchSize][patchSize] */
#define c_runData   5   /* ambient light, rendering mode, screen pos etc. */
#define c_message   6   /* message; size; [ words ] */
#define c_map       7 
#define c_mapAck    8   /* count */
#define c_message2  9   /* message; p1; p2; size; [ words ] */
#define c_start     10
#define c_done      11

#define c_object_ack 90 + c_object
#define c_light_ack 90 + c_light
#define c_runData_ack 90 + c_runData
#define c_start_ack 90 + c_start

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

/* the attribute word contains gross attributes of the object as */
/* bits. The relevant words in the object descriptor expand upon these */

#define a_spec     0x01  /* use lambert / use phong and reflect */
#define a_frac     0x02  /* refract on surface */
#define a_bound0   0x04  /* bounded in 1 dimension */
#define a_bound1   0x08  /* bounded in 2 dimension */
#define a_tex      0x10  /* texture map ? */
#define a_bump     0x20  /* bump map    ? */

typedef struct {
    int x;
    int y;
    int w;
    int h;
} render;

typedef struct {
    int x;
    int y;
    int patchWidth;
    int patchHeight;
    int worker;
    /* pixels [patchWidth][patchHeight]*/
} patch;

typedef struct {
    int type;       /* type field of all objects */
    int attr;       /* attribute field of all object */
    float kdR;      /* coeffs of diffuse reflection R G B */
    float kdG;
    float kdB;
    float ks;       /* coeff of specular reflection ( for reflections) */
    float kg;       /* coeff of gloss for phong shading */
    float xmitR;    /* coeffs of transmission R G B */
    float xmitG;
    float xmitB;
    float power;    /* power for phong shading */
    float refix;    /* refractive index (relative) */
    union {
        struct {
            float rad;
            float x;
            float y;
            float z;
        } sphere;
        struct {
            float x;    /* center of the ellipsoid */
            float y;
            float z;
            float a11;  /* transformation matrix of the ellipsoid */
            float a12;  /* from gobal to local coord_ */
            float a13;
            float a21;
            float a22;
            float a23;
            float a31;
            float a32;
            float a33;
            float radx; /* size on x axis ( of the new system ) */
            float rady; /* size on y axis ( of the new system ) */
            float radz; /* size on z axis ( of the new system ) */
        } ellipsoid;
        struct {
            float x;    /* center of the circle */
            float y;
            float z;
            float a11;  /* transformation matrix of the ellipsoid */
            float a12;  /* from gobal to local coord_ */
            float a13;
            float a21;
            float a22;
            float a23;
            float a31;
            float a32;
            float a33;
            float rada; /* size of the circle at the beginning */
            float co_radb;/* size of the circle at the end */
            float len;  /* length of the cone on new z axis */
        } circle;
        struct {
            float x;
            float y;
            float z;
            float ux;
            float uy;
            float uz;
            float vx;
            float vy;
            float vz;
            float wx;
            float wy;
            float wz;
            float sizeu;
            float sizev;
        } plane;
        struct {
            float x;
            float y;
            float z;
            float sizex;
            float sizey;
        } xyplane;
        struct {
            float x;
            float y;
            float z;
            float sizex;
            float sizez;
        } xzplane;
        struct {
            float x;
            float y;
            float z;
            float sizey;
            float sizez;
        } yzplane;
    } u;
} object;

typedef struct {
    float ir;   /* light intensity fields  R G B */
    float ig;
    float ib;
    float dx;   /* -- light direction cosines x y z */
    float dy;   /* this is a quick'n'tacky shading scheme */
    float dz;
} light;

typedef struct {
    float r;
    float g;
    float b;
} _colour;

typedef struct {
    _colour ambient;
    int renderingMode;
    int runPatchsize;
    int scaleFactor;
    float screenOrg[3];
    float screenX[3];
    float screenY[3];
    float pinhole[3];
} _rundata;
