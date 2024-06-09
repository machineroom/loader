#  MAKEFILE for Loader

#macihenroomfiddling@gmail.com
#Uses normal GNU make on Linux with dosbox to run legacy LSC compiler
#LSC comes from http://www.classiccmp.org/transputer/software/languages/ansic/lsc/

.SUFFIXES: .C .TAL .TLD .ARR .EXE
.PHONY: lsc_debug
all: loader MANDEL.TLD RAYTRACE.TLD FLBOOT.TLD FLLOAD.TLD IDENT.TLD

LSC93=${HOME}/lsc-V93
LSC93_BIN=d:\BIN
LSC93_INCLUDE=d:\include

LSC=$(LSC93)
LSC_BIN=$(LSC93_BIN)
LSC_INCLUDE=$(LSC93_INCLUDE)

#lsc c rule
%.TAL : %.C
	dosbox -c "mount D $(LSC)" -c "mount C `pwd`" -c "C:" -c "$(LSC_BIN)\pp.exe $*.C -I $(LSC_INCLUDE) > PP.OUT" -c "exit"
	cat PP.OUT
	#-c (no debugging information), -f1(ANSI f.p), -p0 (for any processor), -r(relocatable)
	dosbox -c "mount D $(LSC)" -c "mount C `pwd`" -c "C:" -c "$(LSC_BIN)\tcx $* -f1 -p0 -r > CC.OUT" -c "exit"
	cat CC.OUT

#tcode assemble rule
%.TLD : %.TAL
	dosbox -c "mount D $(LSC)" -c "mount C `pwd`" -c "C:" -c "$(LSC_BIN)\tasm.exe $*.TAL -c > ASM.OUT" -c "exit"
	cat ASM.OUT
	dosbox -c "mount D $(LSC)" -c "mount C `pwd`" -c "C:" -c "$(LSC_BIN)\vtlnk $*.LNK > LNK.OUT" -c "exit"
	cat LNK.OUT

#tcode assemble rule
%.TRL : %.TAL 
	dosbox -c "mount D $(LSC)" -c "mount C `pwd`" -c "C:" -c "$(LSC_BIN)\tasm $* -c > ASM.OUT" -c "exit"
	cat ASM.OUT

FRAMEBUF.T4H : FRAMEBUF.OCC PROTOCOL.INC B438.INC
	$(DB) -c "$(OCCAM) FRAMEBUF $(CPU) /h /o FRAMEBUF.T4H $(OCCOPT) > 4.out" -c "exit"
	-grep --color "Warning\|Error\|Serious" 4.OUT

lsc_debug: 
	dosbox -c "mount D $(LSC)" -c "mount C `pwd`" -c "D:" -c "cd $(LSC_BIN)"

# g++ throws lots of compile errors for gpiolib
LIBGPIO=gpiolib.o gpiochip_rp1.o util.o
%.o : %.c
	gcc -O3 -fPIC -o $@ -c $^

libgpio.a: $(LIBGPIO)
	ar rcs $@ $^

loader : main.c load_mandel.c load_raytrace.c mcommon.h rcommon.h lkio_c011.c c011.c libgpio.a
	g++ -std=c++20 -g -O0 main.c load_mandel.c load_raytrace.c lkio_c011.c c011.c -lm -lbcm2835 -lgflags -L. -lgpio -o $@

native_raytrace : RAYTRACE.C conc_native.c lkio_native.c load_raytrace.c B438_native.c
	gcc -o native_raytrace -std=c11 -g -x c -O0 -DNATIVE=1 `sdl2-config --cflags --libs` -lm B438_native.c conc_native.c lkio_native.c load_raytrace.c RAYTRACE.C

clean:
	rm -f *.OBJ
	rm -f *.BIN
	rm -f *.PP
	rm -f *.TRL
	rm -f *.TLD
	rm -f *.MAP
	rm -f MANDEL.TAL
	rm -f MLIBP.TAL
	rm -f B438.TAL

MANDEL.TAL:  MANDEL.C mcommon.h B438.h
MANDEL.TLD:  MANDEL.TAL MLIBP.TRL B438.TRL MANDEL.LNK

RAYTRACE.TAL:  RAYTRACE.C rcommon.h B438.h
RAYTRACE.TLD:  RAYTRACE.TAL MLIBP.TRL B438.TRL RAYTRACE.LNK

B438.TAL: B438.C B438.h
B438.TRL:  B438.TAL

MLIBP.TAL: MLIBP.C
MLIBP.TRL: MLIBP.TAL

FLBOOT.ARR: FLBOOT.TLD
FLBOOT.TLD: FLBOOT.TAL

FLLOAD.ARR: FLLOAD.TLD
FLLOAD.TLD: FLLOAD.TAL

IDENT.ARR : IDENT.TLD
IDENT.TLD : IDENT.TAL


