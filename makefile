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

BC=/home/james/transputer/bc
LSC89=/home/james/transputer/lsc-V89.1
LSC89_BIN=d:\exe

LSC93=/home/james/transputer/lsc-V93.1
LSC93_BIN=d:\bin

# change these according to your system. Keep them SHORT!
BCLIB=D:\lib
BCINC=D:\include

#lsc c rule
%.TAL : %.C
	dosbox -c "mount D $(LSC89)" -c "mount C `pwd`" -c "C:" -c "$(LSC89_BIN)\pp.exe $*.C" -c exit
# no debugging information, for any processor, relocatable
	dosbox -c "mount D $(LSC89)" -c "mount C `pwd`" -c "C:" -c "$(LSC89_BIN)\tcx $* -cf1p0r" -c exit

#tcode assemble rule
%.TLD : %.TAL
	dosbox -c "mount D $(LSC89)" -c "mount C `pwd`" -c "C:" -c "$(LSC89_BIN)\tasm.exe $* -cv" -c exit
	dosbox -c "mount D $(LSC89)" -c "mount C `pwd`" -c "C:" -c "$(LSC89_BIN)\vtlnk $*.lnk" -c exit

#tcode assemble rule
%.TRL : %.TAL
	dosbox -c "mount D $(LSC93)" -c "mount C `pwd`" -c "C:" -c "$(LSC93_BIN)\ttasm $* -cv" -c exit

#rule to make c arrays from .tld files
%.ARR : %.TLD
	dosbox -c "mount C `pwd`" -c "C:" -c "ltoc $*" -c exit
	dosbox -c "mount C `pwd`" -c "C:" -c "mkarr $*" -c exit

man: man.exe

man.exe: MAN.OBJ SCREEN2.OBJ LKIO.OBJ
	dosbox -c "mount D $(BC)" -c "mount C `pwd`" -c "C:" -c "D:\bin\tlink -v  $(BCLIB)\C0S.OBJ MAN.OBJ SCREEN2.OBJ LKIO.OBJ,MAN.EXE,,tchrts $(BCLIB)\emu $(BCLIB)\maths $(BCLIB)\cs" -c exit

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

MAN.OBJ:  MAN.C SRESET.ARR FLBOOT.ARR FLLOAD.ARR IDENT.ARR MANDEL.ARR SMALLMAN.ARR
	dosbox -c "mount D $(BC)" -c "mount C `pwd`" -c "C:" -c "D:\bin\bcc -c -v -ms -Fs -w999 -I$(BCINC) MAN.C" -c exit

LKIO.OBJ:    LKIO.ASM
	dosbox -c "mount D $(BC)" -c "mount C `pwd`" -c "C:" -c "d:\bin\tasm -zi LKIO.ASM" -c exit

SCREEN2.OBJ:    SCREEN2.ASM
	dosbox -c "mount D $(BC)" -c "mount C `pwd`" -c "C:" -c "d:\bin\tasm -zi SCREEN2.ASM" -c exit

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

FLLOAD.ARR: FLLOAD.TLD
FLLOAD.TLD: FLLOAD.TAL

IDENT.ARR: IDENT.TLD
IDENT.TLD: IDENT.TAL


