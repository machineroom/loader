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

lsc_debug: 
	dosbox -c "mount D $(LSC)" -c "mount C `pwd`" -c "D:" -c "cd $(LSC_BIN)"

loader : main.c lkio_c011.c c011.c common.h
	g++ -g -O0 main.c lkio_c011.c c011.c -lm -lbcm2835 -lgflags -o $@

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


