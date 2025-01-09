# vim:set ts=8 sts=8 sw=8 tw=0:
#
# アーキテクチャ依存 (DOS/Windows)
#
# Last Change:	29-Nov-2003.
# Written By:	MURAOKA Taro <koron@tka.att.ne.jp>
# Maintainer:	MURAOKA Taro <koron@tka.att.ne.jp>

srcdir = .\src\				#
objdir = .\build\object\		#
outdir = .\build\			#
# Borlandのmakeでは後ろにコメントを付けることで行末に\を含めることができる

CP = copy
MKDIR = mkdir
RM = del /F /Q
RMDIR = rd /S /Q

O		= obj
EXE		= .exe
CONFIG_DEFAULT	= compile\config_default.mk
CONFIG_IN	= compile\config.mk.in
