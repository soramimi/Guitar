# vim:set ts=8 sts=8 sw=8 tw=0:
#
# デフォルトコンフィギュレーションファイル
#
# Last Change:	19-Jun-2004.
# Base Idea:	AIDA Shinra
# Maintainer:	MURAOKA Taro <koron@tka.att.ne.jp>

srcdir = ./src/
objdir = ./build/object/
outdir = ./build/

##############################################################################
# インストールディレクトリの設定
#
prefix		= /usr/local
bindir		= $(prefix)/bin
libdir		= $(prefix)/lib
incdir		= $(prefix)/include
# 警告: $(dictdir)と$(docdir)はアンインストール実行時にディレクトリごと消去
# されます。
dictdir		= $(prefix)/share/migemo
docdir		= $(prefix)/doc/migemo

##############################################################################
# コマンド設定
#
RM		= rm -f
CP		= cp
MKDIR		= mkdir -p
RMDIR		= rm -rf
CTAGS		= ctags
HTTP		= curl -O
#HTTP		= wget
PERL		= perl
BUNZIP2		= bzip2 -d
GUNZIP		= gzip -d
FILTER_CP932	= qkc -q -u -s
FILTER_EUCJP	= qkc -q -u -e
FILTER_UTF8	= iconv -t utf-8 -f cp932
#FILTER_CP932	= nkf -s
#FILTER_EUCJP	= nkf -e
INSTALL		= /usr/bin/install -c
#INSTALL	= /usr/ucb/install -c
INSTALL_PROGRAM	= $(INSTALL) -m 755
INSTALL_DATA	= $(INSTALL) -m 644

##############################################################################
# 定数
#
O		= o
EXE		=
CONFIG_DEFAULT	= compile/config_default.mk
CONFIG_IN	= compile/config.mk.in
