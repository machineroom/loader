#include <conc.h>
#include <math.h>

#pragma asm
    .T800
    .NOEMULATE
#pragma endasm

/*#define TRUE_COLOUR*/

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
#ifdef TRUE_COLOUR
#define mon_memInit         (unsigned int)128
#else
#define mon_memInit         (unsigned int)512
#endif
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

#define IMS_332_BPP_8		            0x300000
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
#ifdef TRUE_COLOUR
    int multiplier = 2;
#else
    int multiplier = 1;
#endif
    clock = 5; /* 5MHz from TRAM */
    pllMultiplier = mon_frequency/clock;
    pllMultiplier = pllMultiplier * multiplier; /* 24bpp interleaved mode -> clock=2*dot rate */
    IMS_332_WriteRegister (IMS_332_REGBOOT,         pllMultiplier | IMS_332_BOOTCLOCKPLL);
    IMS_332_WriteRegister (IMS_332_REGCSRA,         0);
    IMS_332_WriteRegister (IMS_332_REGCSRB,         0xB); /* 1011 Split SAM, Sync on green, External pixel sampling */
    IMS_332_WriteRegister (IMS_332_REGLINETIME,	    mon_lineTime*multiplier);
    IMS_332_WriteRegister (IMS_332_REGHALFSYNCH,    mon_halfSync*multiplier);
    IMS_332_WriteRegister (IMS_332_REGBACKPORCH,    mon_backPorch*multiplier);
    IMS_332_WriteRegister (IMS_332_REGDISPLAY,      mon_display*multiplier);
    IMS_332_WriteRegister (IMS_332_REGSHORTDIS,	    mon_shortDisplay*multiplier);
    IMS_332_WriteRegister (IMS_332_REGVDISPLAY,     mon_vDisplay);
    IMS_332_WriteRegister (IMS_332_REGVBLANK,	    mon_vBlank);
    IMS_332_WriteRegister (IMS_332_REGVSYNC,		mon_vSync);
    IMS_332_WriteRegister (IMS_332_REGVPREEQUALIZE, mon_vPreEqualize);
    IMS_332_WriteRegister (IMS_332_REGVPOSTEQUALIZE,mon_vPostEqualize);
    IMS_332_WriteRegister (IMS_332_REGBROADPULSE,	mon_broadPulse*multiplier);
    IMS_332_WriteRegister (IMS_332_REGMEMINIT, 	    mon_memInit*multiplier);
    IMS_332_WriteRegister (IMS_332_REGXFERDELAY,	mon_xferDelay*multiplier);
    IMS_332_WriteRegister (IMS_332_REGLINESTART,	mon_lineStart*multiplier);
    IMS_332_WriteRegister (IMS_332_REGCOLORMASK,    0xFFFFFFFF);
    CSRA = 0;
    CSRA |= IMS_332_CSRA_DISABLECURSOR;
#ifdef TRUE_COLOUR
    CSRA |= IMS_335_BPP24;
#else
    CSRA |= IMS_332_BPP_8;
#endif
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

/*void write_pixels (int x, int y, int count, int *pixels) {*/
void write_pixels (x, y, count, pixels)
int x, y, count, *pixels;
{
    int *a = (int *)0x80400000;
    int i;
    a += (y*640)+x;
    for (i=0; i < count; i++) {
        *a++ = pixels[i];
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

void set_palette (int index, unsigned char red, unsigned char green, unsigned char blue) {
    unsigned int red32 = red;
    unsigned int green32 = green;
    IMS_332_WriteRegister(IMS_332_REGLUTBASE + (index & 0xff),
                (red32 << 16) |
                (green32 << 8) |
                blue);
}

void setupGfx(void) {
    int i;
    unsigned char r,g,b;
    resetB438();
    IMS_332_Init();
#ifdef TRUE_COLOUR
    /*bt709Gamma();*/
    poke_words(0, 640*480, 0);
    poke_words(0,640*20,0xFF);/*blue*/
    poke_words(640*20,640*20,0xFF00);/*green*/
    poke_words(640*40,640*20,0xFF0000);/*red*/
    poke_words(640*60,640*20,0xFF00FF);/*pink*/
#else
    /* a somewhat satisfying, but very ripped off palette */
    for (i = 0; i < 256; i++)
    {
        r = 13*(256-i) % 256;
        g = 7*(256-i) % 256;
        b = 11*(256-i) % 256;
        set_palette(i,r,g,b);
    }

    poke_words(0x80400000, 640*480/4, 0);
    poke_words(0x80400000+(640*2*4),640/2,0x02020202);
    poke_words(0x80400000+(640*4*4),640/2,0x01010101);
    poke_words(0x80400000+(640*6*4),640/2,0x03030303);
    poke_words(0x80400000+(640*8*4),640/2,0x04040404);
    poke_words(0x80400000+(640*10*4),640/2,0x05050505);
#endif
}
