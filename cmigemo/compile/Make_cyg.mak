# vim:set ts=8 sts=8 sw=8 tw=0:
#
# Cygwin用Makefile
#
# Last Change:	28-Oct-2003.
# Base Idea:	AIDA Shinra
# Maintainer:	MURAOKA Taro <koron@tka.att.ne.jp>

##############################################################################
# 環境に応じてこの変数を変更する
#
DLLNAME	= cygmigemo1.dll
libmigemo_LIB = $(outdir)libmigemo.dll.a
libmigemo_DSO = $(outdir)$(DLLNAME)
EXEEXT = .exe
CFLAGS_MIGEMO =
LDFLAGS_MIGEMO =

include config.mk
include compile/unix.mak
include src/depend.mak
include compile/clean_unix.mak
include compile/clean.mak

##############################################################################
# 環境に応じてライブラリ構築法を変更する
#
$(libmigemo_LIB): $(libmigemo_DSO)
$(libmigemo_DSO): $(libmigemo_OBJ) $(srcdir)migemo.def
	dllwrap -o $(libmigemo_DSO) --dllname $(DLLNAME) --implib $(libmigemo_LIB) --def $(srcdir)migemo.def $(libmigemo_OBJ)

install-lib: $(libmigemo_DSO) $(libmigemo_LIB)
	$(INSTALL_DATA)		$(libmigemo_LIB) $(libdir)
	$(INSTALL_PROGRAM)	$(libmigemo_DSO) $(bindir)

uninstall-lib:
	$(RM) $(bindir)/$(libmigemo_DSO)
	$(RM) $(libdir)/$(libmigemo_LIB)

dictionary:
	cd dict && $(MAKE) cyg
