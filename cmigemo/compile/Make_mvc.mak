# vim:set ts=8 sts=8 sw=8 tw=0:
#
# Visual C++—p Makefile
#
# Maintainer:	MURAOKA Taro <koron@tka.att.ne.jp>

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

!if "$(_NMAKE_VER)" == "9.00.20706.01"
MSVC_VER = 9.0
!endif
!if "$(_NMAKE_VER)" == "9.00.21022.08"
MSVC_VER = 9.0
!endif
!if "$(_NMAKE_VER)" == "9.00.30729.01"
MSVC_VER = 9.0
!endif

!if "$(MSVC_VER)" == "9.0"
DEFINES = $(DEFINES) -D_CRT_SECURE_NO_WARNINGS
CFLAGS_OPTIMIZE =
!else
CFLAGS_OPTIMIZE = -G6
!endif

!ifndef DEBUG
DEFINES = $(DEFINES) -DNDEBUG
CFLAGS	= $(CFLAGS_OPTIMIZE) -W3 -O2 -MT
LDFLAGS =
!else
DEFINES = $(DEFINES) -D_DEBUG
CFLAGS	= -Zi -W3 -Od -MTd
LDFLAGS = -DEBUG
!endif
DEFINES	= -DWIN32 -D_WINDOWS -D_CONSOLE -D_MBCS $(DEFINES)
CFLAGS	= -nologo -I $(srcdir) $(CFLAGS) $(DEFINES) $(CFLAGS_MIGEMO)
LDFLAGS = -nologo $(LDFLAGS) $(LDFLAGS_MIGEMO)
LIBS	=

LD = link.exe
RC = rc.exe

rel: dirs $(outdir)cmigemo.exe

dirs:
	@for %i IN ($(outdir) $(objdir)) do @if not exist %inul $(MKDIR) %i

{$(srcdir)}.c{$(objdir)}.$(O):
	$(CC) $(CFLAGS) -Fo$@ -c $<

$(outdir)cmigemo.exe: $(libmigemo_LIB) $(objdir)main.obj
	$(LD) $(LDFLAGS) -OUT:$@ $(libmigemo_LIB) $(LIBS) $(objdir)main.obj

$(objdir)migemo.res: $(srcdir)migemo.rc $(srcdir)resource.h
	$(RC) /i $(srcdir) /fo $@ $(srcdir)migemo.rc

$(libmigemo_LIB): $(libmigemo_DSO)
$(libmigemo_DSO): $(libmigemo_OBJ) $(srcdir)migemo.def $(objdir)migemo.res
	$(LD) $(LDFLAGS) -OUT:$@ $(libmigemo_OBJ) $(LIBS) $(objdir)migemo.res -DLL -DEF:$(srcdir)migemo.def -MAP:$(outdir)migemo.map

dictionary: cd-dict msvc
	cd ..

cd-dict:
	cd dict
