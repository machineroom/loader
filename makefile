#  MAKEFILE for Mandelbrot program.
#  Set Borland C compiler and Logical System C compiler path.
#
# Heavily altered by Axel Muhr in 2009
# + corrected tlink libs-order to circumvent float error
# + added make rule for screen2.obj
#
# Caveat: Keep your path to the Borland tools short, else you'll
#         reach the 128 chars-per-commandline limit quick!
#
#         To execute this make-file you'll need the 1989 version
#         of the LSC compiler and use the supplied 'lscmake.exe'
#         (from the 1993 version) for this.

#macihenroomfiddling@gmail.com Oct 2019 started on Linux port
#LSC 89 comes from http://www.classiccmp.org/transputer/software/languages/ansic/lsc/

.SUFFIXES: .C .TAL .TLD .ARR .EXE
.PHONY: lsc_debug
all: man

LSC89=/home/pi/lsc-V89.1
LSC89_BIN=d:\exe

LSC93=/home/pi/lsc-V93.1
LSC93_BIN=d:\bin

LSC=$(LSC89)
LSC_BIN=$(LSC89_BIN)

#lsc c rule
%.TAL : %.C
	dosbox -c "mount D $(LSC)" -c "mount C `pwd`" -c "C:" -c "$(LSC_BIN)\pp.exe $*.C > PP.OUT" -c "exit"
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

#rule to make c arrays from .tld files
%.ARR : %.TLD
	dosbox -c "mount C `pwd`" -c "C:" -c "ltoc $*" -c "mkarr $*" -c "exit"

lsc_debug: 
	dosbox -c "mount D $(LSC)" -c "mount C `pwd`" -c "D:" -c "cd $(LSC_BIN)"
    
man : sdl_man.c lkio_c011.c  c011.c FLBOOT.ARR FLLOAD.ARR IDENT.ARR MANDEL.ARR
	gcc -O2 sdl_man.c lkio_c011.c  c011.c -lSDL2 -lm -lbcm2835 -o $@

clean:
	rm -f *.OBJ
	rm -f *.BIN
	rm -f *.ARR
	rm -f *.PP
	rm -f *.TRL
	rm -f *.TLD
	rm -f *.MAP
	rm -f MANDEL.TAL
	rm -f MLIBP.TAL
	rm -f MAN.EXE

MANDEL.TAL:  MANDEL.C
MANDEL.TLD:  MANDEL.TAL MLIBP.TRL MANDEL.LNK
MANDEL.ARR:  MANDEL.TLD

MLIBP.TAL: MLIBP.C
MLIBP.TRL: MLIBP.TAL

FLBOOT.ARR: FLBOOT.TLD
FLBOOT.TLD: FLBOOT.TAL

FLLOAD.ARR: FLLOAD.TLD
FLLOAD.TLD: FLLOAD.TAL

IDENT.ARR : IDENT.TLD
IDENT.TLD : IDENT.TAL


