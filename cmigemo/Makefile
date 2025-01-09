# vi:set ts=8 sts=8 sw=8 tw=0:
#
# C/Migemo Makefile
#
# Maintainer:	MURAOKA Taro <koron.kaoriya@gmail.com>

default: tags

tags: src/*.c src/*.h
	ctags src/*.c src/*.h

##############################################################################
# for Borland C 5
#
bc: bc-rel
bc-all: bc-rel bc-dict
bc-rel:
	$(MAKE) -f compile\Make_bc5.mak
bc-dict:
	$(MAKE) -f compile\Make_bc5.mak dictionary
bc-clean:
	$(MAKE) -f compile\Make_bc5.mak clean
bc-distclean:
	$(MAKE) -f compile\Make_bc5.mak distclean

##############################################################################
# for Cygwin
#
cyg: cyg-rel
cyg-all: cyg-rel cyg-dict
cyg-rel:
	$(MAKE) -f compile/Make_cyg.mak
cyg-dict:
	$(MAKE) -f compile/Make_cyg.mak dictionary
cyg-install: cyg-all
	$(MAKE) -f compile/Make_cyg.mak install
cyg-uninstall:
	$(MAKE) -f compile/Make_cyg.mak uninstall
cyg-clean:
	$(MAKE) -f compile/Make_cyg.mak clean
cyg-distclean:
	$(MAKE) -f compile/Make_cyg.mak distclean

##############################################################################
# for MinGW
#
mingw: mingw-rel
mingw-all: mingw-rel mingw-dict
mingw-rel:
	$(MAKE) -f compile/Make_mingw.mak
mingw-dict:
	$(MAKE) -f compile/Make_mingw.mak dictionary
mingw-install: mingw-all
	$(MAKE) -f compile/Make_mingw.mak install
mingw-uninstall:
	$(MAKE) -f compile/Make_mingw.mak uninstall
mingw-clean:
	$(MAKE) -f compile/Make_mingw.mak clean
mingw-distclean:
	$(MAKE) -f compile/Make_mingw.mak distclean

##############################################################################
# for GNU/gcc (Linux and others)
#	(Tested on Vine Linux 2.1.5)
#
gcc: gcc-rel
gcc-all: gcc-rel gcc-dict
gcc-rel:
	$(MAKE) -f compile/Make_gcc.mak
gcc-dict:
	$(MAKE) -f compile/Make_gcc.mak dictionary
gcc-install: gcc-all
	$(MAKE) -f compile/Make_gcc.mak install
gcc-uninstall:
	$(MAKE) -f compile/Make_gcc.mak uninstall
gcc-clean:
	$(MAKE) -f compile/Make_gcc.mak clean
gcc-distclean:
	$(MAKE) -f compile/Make_gcc.mak distclean

##############################################################################
# for Microsoft Visual C
#
msvc: msvc-rel
msvc-all: msvc-rel msvc-dict
msvc-rel:
	$(MAKE) /nologo /f compile\Make_mvc.mak
msvc-dict:
	$(MAKE) /nologo /f compile\Make_mvc.mak dictionary
msvc-clean:
	$(MAKE) /nologo /f compile\Make_mvc.mak clean
msvc-distclean:
	$(MAKE) /nologo /f compile\Make_mvc.mak distclean

##############################################################################
# for MacOS X
#
osx: osx-rel
osx-all: osx-rel osx-dict
osx-rel:
	$(MAKE) -f compile/Make_osx.mak
osx-dict:
	$(MAKE) -f compile/Make_osx.mak dictionary
osx-install: osx-all
	$(MAKE) -f compile/Make_osx.mak install
osx-uninstall:
	$(MAKE) -f compile/Make_osx.mak uninstall
osx-clean:
	$(MAKE) -f compile/Make_osx.mak clean
osx-distclean:
	$(MAKE) -f compile/Make_osx.mak distclean

##############################################################################
# for Sun's Solaris/gcc
#	(Tested on Solaris 8)
#
sun: sun-rel
sun-all: sun-rel sun-dict
sun-rel:
	$(MAKE) -f compile/Make_sun.mak
sun-dict:
	$(MAKE) -f compile/Make_sun.mak dictionary
sun-install: sun-all
	$(MAKE) -f compile/Make_sun.mak install
sun-uninstall:
	$(MAKE) -f compile/Make_sun.mak uninstall
sun-clean:
	$(MAKE) -f compile/Make_sun.mak clean
sun-distclean:
	$(MAKE) -f compile/Make_sun.mak distclean
