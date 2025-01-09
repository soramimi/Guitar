# vim:set ts=8 sts=8 sw=8 tw=0:
#
# Borland C 5óp Makefile
#
# Last Change:	19-Oct-2003.
# Adviced By:	MATSUMOTO Yasuhiro
# Maintainer:	MURAOKA Taro <koron@tka.att.ne.jp>

# éQçléëóø:
#	http://www2.justnet.ne.jp/~tyche/bcbbugs/bcc32-option.html
#	http://www2.justnet.ne.jp/~tyche/bcbbugs/ilink32-option.html

default: rel

!include config.mk
!include compile\dos.mak
!include src\depend.mak
!include compile\clean_dos.mak
!include compile\clean.mak
!include dict\dict.mak

libmigemo_LIB = $(outdir)migemo.lib
libmigemo_DSO = $(outdir)migemo.dll
libmigemo_SRC = $(SRC)
libmigemo_OBJ = $(OBJ)

DEFINES	= -DWIN32 -D_CONSOLE
CFLAGS	= -O2 -G -pr -w- -VM -WM $(DEFINES) $(CFLAGS_MIGEMO)
LDFLAGS = -x -Gn -w- $(LDFLAGS_MIGEMO)
LIBS	= import32.lib cw32mt.lib

LD = ilink32

rel: dirs $(outdir)cmigemo.exe

dirs:
	@for %i IN ($(outdir) $(objdir)) do if not exist %inul $(MKDIR) %i

# Without the following, the implicit rule in BUILTINS.MAK is picked up
# for a rule for .c.obj rather than the local implicit rule
.SUFFIXES
.SUFFIXES .c .obj

{$(srcdir)}.c{$(objdir)}.$(O):
	$(CC) $(CFLAGS) -o$@ -c $<

$(outdir)cmigemo.exe: $(libmigemo_LIB) $(objdir)main.obj
	$(LD) $(LDFLAGS) c0x32.obj $(objdir)main.obj, $@, , $(libmigemo_LIB) $(LIBS), ,

$(libmigemo_LIB): $(libmigemo_DSO)
$(libmigemo_DSO): $(libmigemo_OBJ) $(srcdir)migemo.def
	$(LD) $(LDFLAGS) -Tpd -Gi $(libmigemo_OBJ) c0d32.obj, $@, , $(LIBS), $(srcdir)migemo.def,

dictionary: cd-dict bc5
	cd ..

cd-dict:
	cd dict
