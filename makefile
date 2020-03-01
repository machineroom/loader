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

LSC89=/home/pi/lsc-V89.1
LSC89_BIN=d:\exe

LSC93=/home/pi/lsc-V93.1
LSC93_BIN=d:\bin

#lsc c rule
%.TAL : %.C
	dosbox -c "mount D $(LSC89)" -c "mount C `pwd`" -c "C:" -c "$(LSC89_BIN)\pp.exe $*.C" -c exit
    #no debugging information, for any processor, relocatable
	dosbox -c "mount D $(LSC89)" -c "mount C `pwd`" -c "C:" -c "$(LSC89_BIN)\tcx $* -cf1p0r" -c exit

#tcode assemble rule
%.TLD : %.TAL
	dosbox -c "mount D $(LSC89)" -c "mount C `pwd`" -c "C:" -c "$(LSC89_BIN)\tasm.exe $*.TAL -cv > asm.out" -c "$(LSC89_BIN)\vtlnk $*.LNK" -c exit
	cat ASM.OUT

#tcode assemble rule
%.TRL : %.TAL
	dosbox -c "mount D $(LSC93)" -c "mount C `pwd`" -c "C:" -c "$(LSC93_BIN)\ttasm $* -cv" -c exit

#rule to make c arrays from .tld files
%.ARR : %.TLD
	dosbox -c "mount C `pwd`" -c "C:" -c "ltoc $*" -c "mkarr $*" -c exit

man : sdl_man.c lkio_c011.c  c011.c SRESET.ARR FLBOOT.ARR FLLOAD.ARR IDENT.ARR MANDEL.ARR SMALLMAN.ARR EXPLORE.ARR
	gcc -O0 -g sdl_man.c lkio_c011.c  c011.c -lSDL2 -lm -lbcm2835 -o $@

clean:
	rm -f *.OBJ
	rm -f *.BIN
	rm -f *.ARR
	rm -f *.PP
	rm -f *.TRL
	rm -f *.TLD
	rm -f *.MAP
	rm -f MANDEL.TAL
	rm -f SMALLMAN.TAL
	rm -f MLIBS.TAL
	rm -f MLIBP.TAL
	rm -f MAN.EXE

MANDEL.TAL:  MANDEL.C
MANDEL.TLD:  MANDEL.TAL MLIBP.TRL MANDEL.LNK
MANDEL.ARR:  MANDEL.TLD

SMALLMAN.TAL:  SMALLMAN.C
SMALLMAN.TLD:  SMALLMAN.TAL MLIBS.TRL SMALLMAN.LNK
SMALLMAN.ARR:  SMALLMAN.TLD

MLIBS.TAL:  MLIBS.C
MLIBS.TRL:  MLIBS.TAL

MLIBP.TAL: MLIBP.C
MLIBP.TRL: MLIBP.TAL

SRESET.ARR: SRESET.TLD
SRESET.TLD: SRESET.TAL

FLBOOT.ARR: FLBOOT.TLD
FLBOOT.TLD: FLBOOT.TAL

EXPLORE.ARR: EXPLORE.TLD
EXPLORE.TLD: EXPLORE.TAL

FLLOAD.ARR: FLLOAD.TLD
FLLOAD.TLD: FLLOAD.TAL

IDENT.ARR : IDENT.TLD
IDENT.TLD : IDENT.TAL


