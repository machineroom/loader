#  MAKEFILE for Loader

#macihenroomfiddling@gmail.com
#Uses normal GNU make on Linux with dosbox to run legacy LSC compiler
#LSC 89 comes from http://www.classiccmp.org/transputer/software/languages/ansic/lsc/

.SUFFIXES: .C .TAL .TLD .ARR .EXE
.PHONY: lsc_debug
all: loader MANDEL.TLD FLBOOT.TLD FLLOAD.TLD IDENT.TLD

LSC89=${HOME}/lsc-V89.1
LSC89_BIN=d:\exe
LSC89_INCLUDE=d:\include

LSC93=${HOME}/lsc-V93
LSC93_BIN=d:\BIN
LSC93_INCLUDE=d:\include

LSC=$(LSC89)
LSC_BIN=$(LSC89_BIN)
LSC_INCLUDE=$(LSC89_INCLUDE)

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

RAYTRACE.LKU : FRAMEBUF.T4H CNTLSYS.T4H RAYTRACE.T4H
	$(DB) -c "$(LINK) /f RAYTRACE.L4H $(CPU) /h /o RAYTRACE.LKU $(LINKOPT) > 2.out" -c "exit"
	-grep --color "Warning\|Error\|Serious" 2.OUT
	$(DB) -c "ilist /A /T /C /M /W /I /X RAYTRACE.LKU > raytrace.lst" -c "exit"

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

loader : main.c load_mandel.c lkio_c011.c c011.c common.h libgpio.a
	g++ -g -O0 main.c load_mandel.c lkio_c011.c c011.c -lm -lbcm2835 -lgflags -L. -lgpio -o $@

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

MANDEL.TAL:  MANDEL.C common.h B438.h
MANDEL.TLD:  MANDEL.TAL MLIBP.TRL B438.TRL MANDEL.LNK

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


