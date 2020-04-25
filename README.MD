About
-----
This is the source of the "CSA Mandelzoom" tool, created by Computer System Architects (CSA) around 1989.

It maps an existing Transputer network and loads workers onto every T4xx/T8xx found. Integer as well as floatingpoint workers are provided. Graphical output is either CGS, EGA or VGA currently at 16 colors max.


macihenroomfiddling@gmail.com enhancements
------------------------
* Modified build to use GNU make and call dosbox for legacy LSC tools
* Remove x86 asm/dos/int usage and replaced render & input with SDL2 so main app now builds on Linux
* Use more memory to get better performance, especially host rendering
* Added FB option for much faster rendering (~1000x faster than SDL2)
* Must run as root to access timer registers on BCM chip
* TODO:
* Fix race condition? that causes host to not recieve a RSLCOM message and a number of incomplete slices left (worse with more nodes)

Mimicing Axel's results using man.dat auto-render and on-the-fly rendering using FB:
System | 1st | 2nd | notes
-------|-----|-----|------
Axel cube 65*T425 | 2.32s | 8.16s | Mixed 20 & 25MHz parts
13  nodes | 6.74s | 37.34s | Each node is T425@25MHz
36  nodes | 3.41s | 15.79s
121 nodes | 1.71s | 5.04s
230 nodes | 1.72s | 3.85s
Pi3       | 0.19s | 1.00s

230 nodes is _certainly_ the fastest operational T425 system in the world (woot). (121 nodes recorded @ https://youtu.be/16nZVvnu3C4)

Two problems with WX9020:
1. I'm running the HSL cards (that connect 6x links over a fiber to connect racks together) in byte mode. This is documented to be slow but is transparent to software (i.e. it looks like a regular link).
2. The Pi/C011 link is also slow. ispy output gives the HSL links ~130K and the C011 link ~500K. Other links are ~1.8MB/s.

(original DOS version enhanced by Axel):

Prerequisites
-------------
* At least one Transputer (does not make sense without one)
* A x86 PC running [MS|Free|Open|PC]DOS - yes, it's DOS only
* Borland C-compiler (I use BC++ 2.0)
* LSC Transputer cross C-Compiler (use this [link to download](http://www.classiccmp.org/transputer/software/languages/ansic/lsc/lsc-V89.1.tar.gz)

Enhancements
------------
I've added some functions as well as a high-precision timer, mainly used for benchmarking.

But still there are many things to do:
* Better graphics support (higher resoluton, more colors)
* Better Transputer detection (crashed if T2xx or C004 are found)
* Better Timer-Lib. This current is an eval version, manufacturer vanished, can't remove nag-screen.
* Enhance link adaptor support (currently B004 interface only)
* Mouse control?
* ...

Learn more about the tool at [this page](http://www.geekdot.com/basic-transputer-tools)

Several benchmarking results can be found [here](http://www.geekdot.com/lies-damn-lies-and-benchmarks)

In doubt enter "man /?" for options. 



