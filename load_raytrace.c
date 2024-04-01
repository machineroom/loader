#include "load_mandel.h"
#include "rcommon.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "lkio.h"

static void send_object(object *o) {
    if (word_out(c_object) != 0) {
        printf(" -- timeout sending object type\n");
        exit(1);
    }
    if (chan_out((char *)o,sizeof(*o)) != 0) {
        printf(" -- timeout sending object\n");
        exit(1);
    }
}

static void send_light(light *l) {
    if (word_out(c_light) != 0) {
        printf(" -- timeout sending light type\n");
        exit(1);
    }
    if (chan_out((char *)l,sizeof(*l)) != 0) {
        printf(" -- timeout sending light\n");
        exit(1);
    }
}

static void send_rundata(rundata *r) {
    if (word_out(c_runData) != 0) {
        printf(" -- timeout sending rundata type\n");
        exit(1);
    }
    if (chan_out((char *)r,sizeof(*r)) != 0) {
        printf(" -- timeout sending rundata\n");
        exit(1);
    }
}

static void pumpWorldModels(int model, int patchEdge) {
    switch (model) {
        case 1: /* 10 sphere selection */
        {
            float rad = 102.0;
            float y  = -210.0;
            float z = 3910.0;
            float x0 = -210.0;
            float x1 = 3.0;
            float x2 = 210.0;
            object sphere0 = {
            .type = o_sphere,
            .attr = 0,
            .kdR = 0.7,
            .kdG = 0.1,
            .kdB = 0.1,
            .ks = 0.9,
            .u.sphere.rad = rad,
            .u.sphere.x = x0,
            .u.sphere.y = y,
            .u.sphere.z = z
            };
            //out ! c.object; s.size; [ temp FROM 0 FOR s.size ]
            send_object(&sphere0);
            object sphere1 = {
            .type = o_sphere,
            .attr = a_spec,
            .kdR = 0.2,
            .kdG = 0.3,
            .kdB = 0.4,
            .ks = 0.9,
            .kg = 0.95,
            .u.sphere.rad = rad,
            .u.sphere.x = x1,
            .u.sphere.y = y,
            .u.sphere.z = z
            };
            send_object(&sphere1);
            object sphere2 = {
            .type = o_sphere,
            .attr = 0,
            .kdR = 0.1,
            .kdG = 0.9,
            .kdB = 0.1,
            .ks = 0.9,
            .u.sphere.rad = rad,
            .u.sphere.x = x2,
            .u.sphere.y = y,
            .u.sphere.z = z
            };
            send_object(&sphere2);
            printf ("sent 3 spheres\n");
            rad=102.0;
            y=5.0;
            z=3910.0;
            x0=-210.0;
            x1=3.0;
            x2=210.0;
            object sphere3 = {
            .type = o_sphere,
            .attr = a_spec,
            .kdR = 0.1,
            .kdG = 0.4,
            .kdB = 0.1,
            .ks = 0.9,
            .kg = 0.9,
            .u.sphere.rad = rad,
            .u.sphere.x = x0,
            .u.sphere.y = y,
            .u.sphere.z = z
            };
            send_object(&sphere3);
            object sphere4 = {
            .type = o_sphere,
            .attr = a_spec,
            .kdR = 0.3,
            .kdG = 0.1,
            .kdB = 0.1,
            .ks = 0.95,
            .kg = 0.95,
            .u.sphere.rad = rad,
            .u.sphere.x = x1,
            .u.sphere.y = y,
            .u.sphere.z = z
            };
            send_object(&sphere4);
            object sphere5 = {
            .type = o_sphere,
            .attr = a_spec,
            .kdR = 0.3,
            .kdG = 0.1,
            .kdB = 0.3,
            .ks = 0.9,
            .kg = 0.95,
            .u.sphere.rad = rad,
            .u.sphere.x = x2,
            .u.sphere.y = y,
            .u.sphere.z = z
            };
            send_object(&sphere5);
            printf ("sent 3 spheres\n");
            rad=102.0;
            y=210.0;
            z=3910.0;
            x0=-210.0;
            x1=3.0;
            x2=210.0;
            object sphere6 = {
            .type = o_sphere,
            .attr = 0,
            .kdR = 0.7,
            .kdG = 0.7,
            .kdB = 0.1,
            .ks = 0.9,
            .u.sphere.rad = rad,
            .u.sphere.x = x0,
            .u.sphere.y = y,
            .u.sphere.z = z
            };
            send_object(&sphere6);
            object sphere7 = {
            .type = o_sphere,
            .attr = a_spec,
            .kdR = 0.4,
            .kdG = 0.1,
            .kdB = 0.4,
            .ks = 0.9,
            .kg = 0.9,
            .u.sphere.rad = rad,
            .u.sphere.x = x1,
            .u.sphere.y = y,
            .u.sphere.z = z
            };
            send_object(&sphere7);
            object sphere8 = {
            .type = o_sphere,
            .attr = 0,
            .kdR = 0.1,
            .kdG = 0.1,
            .kdB = 0.7,
            .ks = 0.9,
            .u.sphere.rad = rad,
            .u.sphere.x = x2,
            .u.sphere.y = y,
            .u.sphere.z = z
            };
            send_object(&sphere8);
            printf ("sent 3 spheres\n");
            object sphere9 = {
            .type = o_sphere,
            .attr = a_spec | a_frac,
            .kdR = 0,
            .kdG = 0,
            .kdB = 0,
            .ks = 0.3,
            .kg = 0.8,
            .xmitR = 0.7,
            .xmitG = 0.7,
            .xmitB = 0.7,
            .refix = 1.02,
            .power = 10,
            .u.sphere.rad = 100,
            .u.sphere.x = -90,
            .u.sphere.y = -80,
            .u.sphere.z = 3100
            };
            send_object(&sphere9);
            printf ("sent glass sphere\n");
        }
        break;
    }
    if (model != 1) {
        object plane = {
            .type = o_xzplane,
            .attr = a_spec | a_bound0 | a_bound1,
            .kdR = 0.05,
            .kdG = 0.4,
            .kdB = 0.05,
            .ks = 0.7,
            .kg = 0.1,
            .u.xzplane.x = -2000.0,
            .u.xzplane.y = -690.0,
            .u.xzplane.z = 10000.0,
            .u.xzplane.sizex = 4000.0,
            .u.xzplane.sizez = 3000.0,
        };
        send_object (&plane);
        printf ("a plane\n");
    };
    // a yellow sun
    light l1 = {
        .ir = 780.8,
        .ig = 780.8,
        .ib = 710.7,
        .dx = 1.7,
        .dy = 1.6,
        .dz = -0.9
    };
    send_light(&l1);
    printf ("a light\n");
    // a yellow sun
    light l2 = {
        .ir = 780.8,
        .ig = 780.8,
        .ib = 710.7,
        .dx = -1.1,
        .dy = 1.8,
        .dz = -0.3
    };
    send_light(&l2);
    printf ("a light\n");
    float cosTheta = 0.93969;
    float sinTheta = 0.34202;
    float sin_256 = sinTheta * 256.0;
    float cos_256 = cosTheta * 256.0;
    float offX = 0.0;
    float offY = 0.0;
    float offZ = 0.0;
    rundata r = {
        .ambient[0] = 266.0,
        .ambient[1] = 266.0,
        .ambient[2] = 280.0,
        .renderingMode = m_adaptive,
        .runPatchsize = patchEdge,
        .scaleFactor = 1,
        .screenOrg[0] = offX - 256.0,
        .screenOrg[1] = offY - 256.0,
        .screenOrg[2] = offZ + 0.0,
        .screenX[0] = 1.0,
        .screenX[1] = 0.0,
        .screenX[2] = 0.0,
        .screenY[0] = 0.0,
        .screenY[1] = 1.0,  // cosTheta
        .screenY[2] = 0.0,  // sinTheta
        .pinhole[0] = offX + 0.0,
        .pinhole[1] = offY + 0.0,
        .pinhole[2] = offZ + 1024.0
    };
    send_rundata (&r);
    printf ("rundata\n");
}

#define DEBUG
void do_raytrace(void) {
    int patchEdge = 8;
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

    int selection;
    while (true) {
        printf("                                                           \n");
        printf("               RayTracer Image Selection                   \n");
        printf("               =========================                   \n");
        printf("                                                           \n");
        printf("               <1>  10 Sphere Image                        \n");
        printf("               <2>  14 Sphere Image                        \n");
        printf("               <3>  Cone, Ellipsoid Image                  \n");
        printf("               <4>  Cylinder, Sphere Image                 \n");
        printf("               <5>  (small) Cylinder, Sphere Image         \n");
        printf("                                                           \n");
        printf(" Your Selection: ");

        selection = getchar() - '0';
        if (selection >= 1 && selection <= 5) {
            break;
        }
    }
    pumpWorldModels (selection, patchEdge);
    if (word_out(c_start) != 0) {
        printf(" -- timeout sending start\n");
        exit(1);
    }
    printf ("The inmos ray tracer is GO\n");

    // wait for finish
	while (1)
	{
        int len;
        if (word_in(&len) == 0) {      //len in bytes
            int32_t buf[1024];
            if (chan_in ((char *)buf,len) == 0) {
#ifdef DEBUG
                printf ("len=0x%X, buf = 0x%X 0x%X 0x%X 0x%X...0x%X\n",len,buf[0],buf[1],buf[2],buf[3],buf[(len/4)-1]);
#endif
                if (buf[0] == c_stop) {
                    printf ("Finished\n");
                    break;
                }
            }
        }
    }
}