#include "load_mandel.h"
#include "rcommon.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "lkio.h"

static bool get_ack (int expected_ack) {
    int ack;
    if (word_in(&ack) == 0) {
        if (ack == expected_ack) {
            printf ("\tack\n");
            return true;
        } else {
            printf ("got wrong ack. expecting %d got %d\n", expected_ack, ack);
        }
    } else {
        printf ("timed out getting ack\n");
    }
    return false;
}

static void send_object(object *o) {
    printf ("send object\n");
    if (word_out(c_object) != 0) {
        printf(" -- timeout sending object type\n");
        exit(1);
    }
    if (chan_out((char *)o,sizeof(*o)) != 0) {
        printf(" -- timeout sending object\n");
        exit(1);
    }
    if (!get_ack (c_object_ack)) {
        exit(1);
    }
}

static void send_light(light *l) {
    printf ("send light\n");
    if (word_out(c_light) != 0) {
        printf(" -- timeout sending light type\n");
        exit(1);
    }
    if (chan_out((char *)l,sizeof(*l)) != 0) {
        printf(" -- timeout sending light\n");
        exit(1);
    }
    if (!get_ack (c_light_ack)) {
        exit(1);
    }
}

static void send_rundata(rundata *r) {
    printf ("send runData\n");
    if (word_out(c_runData) != 0) {
        printf(" -- timeout sending rundata type\n");
        exit(1);
    }
    if (chan_out((char *)r,sizeof(*r)) != 0) {
        printf(" -- timeout sending rundata\n");
        exit(1);
    }
    if (!get_ack (c_runData_ack)) {
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
            object sphere;
            sphere.type = o_sphere;
            sphere.attr = 0;
            sphere.kdR = 0.7;
            sphere.kdG = 0.1;
            sphere.kdB = 0.1;
            sphere.ks = 0.9;
            sphere.u.sphere.rad = rad;
            sphere.u.sphere.x = x0;
            sphere.u.sphere.y = y;
            sphere.u.sphere.z = z;
            //out ! c.object; s.size; [ temp FROM 0 FOR s.size ]
            send_object(&sphere);
            sphere.type = o_sphere;
            sphere.attr = a_spec;
            sphere.kdR = 0.2;
            sphere.kdG = 0.3;
            sphere.kdB = 0.4;
            sphere.ks = 0.9;
            sphere.kg = 0.95;
            sphere.u.sphere.rad = rad;
            sphere.u.sphere.x = x1;
            sphere.u.sphere.y = y;
            sphere.u.sphere.z = z;
            send_object(&sphere);
            sphere.type = o_sphere;
            sphere.attr = 0;
            sphere.kdR = 0.1;
            sphere.kdG = 0.9;
            sphere.kdB = 0.1;
            sphere.ks = 0.9;
            sphere.u.sphere.rad = rad;
            sphere.u.sphere.x = x2;
            sphere.u.sphere.y = y;
            sphere.u.sphere.z = z;
            send_object(&sphere);
            printf ("sent 3 spheres\n");
            rad=102.0;
            y=5.0;
            z=3910.0;
            x0=-210.0;
            x1=3.0;
            x2=210.0;
            sphere.type = o_sphere;
            sphere.attr = a_spec;
            sphere.kdR = 0.1;
            sphere.kdG = 0.4;
            sphere.kdB = 0.1;
            sphere.ks = 0.9;
            sphere.kg = 0.9;
            sphere.u.sphere.rad = rad;
            sphere.u.sphere.x = x0;
            sphere.u.sphere.y = y;
            sphere.u.sphere.z = z;
            send_object(&sphere);
            sphere.type = o_sphere;
            sphere.attr = a_spec;
            sphere.kdR = 0.3;
            sphere.kdG = 0.1;
            sphere.kdB = 0.1;
            sphere.ks = 0.95;
            sphere.kg = 0.95;
            sphere.u.sphere.rad = rad;
            sphere.u.sphere.x = x1;
            sphere.u.sphere.y = y;
            sphere.u.sphere.z = z;
            send_object(&sphere);
            sphere.type = o_sphere;
            sphere.attr = a_spec;
            sphere.kdR = 0.3;
            sphere.kdG = 0.1;
            sphere.kdB = 0.3;
            sphere.ks = 0.9;
            sphere.kg = 0.95;
            sphere.u.sphere.rad = rad;
            sphere.u.sphere.x = x2;
            sphere.u.sphere.y = y;
            sphere.u.sphere.z = z;
            send_object(&sphere);
            printf ("sent 3 spheres\n");
            rad=102.0;
            y=210.0;
            z=3910.0;
            x0=-210.0;
            x1=3.0;
            x2=210.0;
            sphere.type = o_sphere;
            sphere.attr = 0;
            sphere.kdR = 0.7;
            sphere.kdG = 0.7;
            sphere.kdB = 0.1;
            sphere.ks = 0.9;
            sphere.u.sphere.rad = rad;
            sphere.u.sphere.x = x0;
            sphere.u.sphere.y = y;
            sphere.u.sphere.z = z;
            send_object(&sphere);
            sphere.type = o_sphere;
            sphere.attr = a_spec;
            sphere.kdR = 0.4;
            sphere.kdG = 0.1;
            sphere.kdB = 0.4;
            sphere.ks = 0.9;
            sphere.kg = 0.9;
            sphere.u.sphere.rad = rad;
            sphere.u.sphere.x = x1;
            sphere.u.sphere.y = y;
            sphere.u.sphere.z = z;
            send_object(&sphere);
            sphere.type = o_sphere;
            sphere.kdR = 0.1;
            sphere.kdG = 0.1;
            sphere.kdB = 0.7;
            sphere.ks = 0.9;
            sphere.u.sphere.rad = rad;
            sphere.u.sphere.x = x2;
            sphere.u.sphere.y = y;
            sphere.u.sphere.z = z;
            send_object(&sphere);
            printf ("sent 3 spheres\n");
            sphere.type = o_sphere;
            sphere.attr = a_spec | a_frac;
            sphere.kdR = 0;
            sphere.kdG = 0;
            sphere.kdB = 0;
            sphere.ks = 0.3;
            sphere.kg = 0.8;
            sphere.xmitR = 0.7;
            sphere.xmitG = 0.7;
            sphere.xmitB = 0.7;
            sphere.refix = 1.02;
            sphere.power = 10;
            sphere.u.sphere.rad = 100;
            sphere.u.sphere.x = -90;
            sphere.u.sphere.y = -80;
            sphere.u.sphere.z = 3100;
            send_object(&sphere);
            printf ("sent glass sphere\n");
        }
        break;
    }
    if (model != 1) {
        object plane;
        plane.type = o_xzplane;
        plane.attr = a_spec | a_bound0 | a_bound1;
        plane.kdR = 0.05;
        plane.kdG = 0.4;
        plane.kdB = 0.05;
        plane.ks = 0.7;
        plane.kg = 0.1;
        plane.u.xzplane.x = -2000.0;
        plane.u.xzplane.y = -690.0;
        plane.u.xzplane.z = 10000.0;
        plane.u.xzplane.sizex = 4000.0;
        plane.u.xzplane.sizez = 3000.0;
        send_object (&plane);
        printf ("a plane\n");
    };
    // a yellow sun
    light l;
    l.ir = 780.8;
    l.ig = 780.8;
    l.ib = 710.7;
    l.dx = 1.7;
    l.dy = 1.6;
    l.dz = -0.9;
    send_light(&l);
    printf ("a light\n");
    // a yellow sun
    l.ir = 780.8;
    l.ig = 780.8;
    l.ib = 710.7;
    l.dx = -1.1;
    l.dy = 1.8;
    l.dz = -0.3;
    send_light(&l);
    printf ("a light\n");
    float cosTheta = 0.93969;
    float sinTheta = 0.34202;
    float sin_256 = sinTheta * 256.0;
    float cos_256 = cosTheta * 256.0;
    float offX = 0.0;
    float offY = 0.0;
    float offZ = 0.0;
    rundata r;
    r.ambient[0] = 266.0;
    r.ambient[1] = 266.0;
    r.ambient[2] = 280.0;
    r.renderingMode = m_adaptive;
    r.runPatchsize = patchEdge;
    r.scaleFactor = 1;
    r.screenOrg[0] = offX - 256.0;
    r.screenOrg[1] = offY - 256.0;
    r.screenOrg[2] = offZ + 0.0;
    r.screenX[0] = 1.0;
    r.screenX[1] = 0.0;
    r.screenX[2] = 0.0;
    r.screenY[0] = 0.0;
    r.screenY[1] = 1.0;  // cosTheta
    r.screenY[2] = 0.0;  // sinTheta
    r.pinhole[0] = offX + 0.0;
    r.pinhole[1] = offY + 0.0;
    r.pinhole[2] = offZ + 1024.0;
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
    printf ("send c_start\n");
    if (word_out(c_start) != 0) {
        printf(" -- timeout sending start\n");
        exit(1);
    }
    if (!get_ack (c_start_ack)) {
        exit(1);
    }
    printf ("The inmos ray tracer is GO\n");

    // wait for finish
	while (1)
	{
        int cmd;
        if (word_in(&cmd) == 0) {
            switch (cmd) {
                case c_done:
                    printf ("c_done\n");
                break;
                case c_stop:
                    printf ("c_stop\n");
                    break;
                case c_message:
                    {
                        int size;
                        char buf[1204];
                        word_in (&size);
                        assert (size<sizeof(buf));
                        chan_in(buf, size);
                        printf ("message : %s\n", buf);
                    }
                    break;
                case c_message2:
                    {
                        int p1,p2,size;
                        char buf[1204];
                        word_in (&p1);
                        word_in (&p2);
                        word_in (&size);
                        assert (size<sizeof(buf));
                        chan_in(buf, size);
                        printf ("message2 : %d %d %s\n", p1,p2,buf);
                    }
                    break;
                default:
                    printf ("duff command (%d)\n", cmd);
                    break;
            }
        }
    }
}