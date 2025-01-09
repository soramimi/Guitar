# vim:set ts=8 sts=8 sw=8 tw=0:
#
# Configuration file.
#
# Last Change: 08-Dec-2004.
# Maintainer: MURAOKA Taro <koron@tka.att.ne.jp>

srcdir = ./src/
objdir = ./build/object/
outdir = ./build/

##############################################################################
# Install directories
#
prefix = /usr/local
bindir = $(prefix)/bin
libdir = $(prefix)/lib
incdir = $(prefix)/include
# WARNING: Directories $(dictdir) and $(docdir) will be deleted whole the
# directory when unintall.
dictdir = $(prefix)/share/migemo
docdir = $(prefix)/doc/migemo

##############################################################################
# Commands
#
RM = rm -f
CP = cp
MKDIR = mkdir -p
RMDIR = rm -rf
CTAGS = ctags
HTTP = curl -O
PERL = perl
BUNZIP2 = bzip2 -d
GUNZIP = gzip -d
FILTER_CP932 = nkf -x -s
FILTER_EUCJP = nkf -x -e
FILTER_UTF8 = iconv -t utf-8 -f cp932
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644

##############################################################################
# Constants
#
O = o
EXE =
CONFIG_DEFAULT = compile/config_default.mk
CONFIG_IN = compile/config.mk.in
