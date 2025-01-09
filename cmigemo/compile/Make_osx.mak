# vim:set ts=8 sts=8 sw=8 tw=0:
#
# MacOSX用Makefile
#
# Last Change:	19-Oct-2003.
# Maintainer:	MURAOKA Taro <koron@tka.att.ne.jp>

##############################################################################
# 環境に応じてこの変数を変更する
#
libmigemo_LIB	= libmigemo.1.1.0.dylib
libmigemo_DSO	= libmigemo.1.dylib
libmigemo	= libmigemo.dylib
EXEEXT		=
CFLAGS_MIGEMO	= -fPIC
LDFLAGS_MIGEMO	=

include config.mk
include compile/unix.mak
include src/depend.mak
include compile/clean_unix.mak
include compile/clean.mak

##############################################################################
# 環境に応じてライブラリ構築法を変更する
#
$(libmigemo_LIB): $(libmigemo_DSO)
$(libmigemo_DSO): $(libmigemo_OBJ)
	$(CC) -dynamiclib -install_name $(libdir)/$@ -o $(libmigemo_LIB) $(libmigemo_OBJ)
	$(RM) $@ $(libmigemo)
	ln -s $(libmigemo_LIB) $@
	ln -s $(libmigemo_LIB) $(libmigemo)

install-lib: $(libmigemo_DSO)
	$(INSTALL_PROGRAM) $(libmigemo_LIB) $(libdir)
	$(RM) $(libdir)/$(libmigemo_DSO) $(libdir)/$(libmigemo)
	ln -s $(libmigemo_LIB) $(libdir)/$(libmigemo_DSO)
	ln -s $(libmigemo_LIB) $(libdir)/$(libmigemo)

uninstall-lib:
	$(RM) $(libdir)/$(libmigemo_DSO)
	$(RM) $(libdir)/$(libmigemo_LIB)
	$(RM) $(libdir)/$(libmigemo)

dictionary:
	cd dict && $(MAKE) osx
