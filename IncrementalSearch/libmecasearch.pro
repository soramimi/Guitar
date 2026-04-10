
QMAKE_PROJECT_DEPTH = 0

DESTDIR = $$PWD
CONFIG(debug,debug|release):TARGET = mecasearchd
CONFIG(release,debug|release):TARGET = mecasearch
TEMPLATE = lib
CONFIG += staticlib

CPP_STD = c++17

CONFIG += $$CPP_STD nostrip debug_info static

INCLUDEPATH += $$PWD/mecab/mecab/src
INCLUDEPATH += $$PWD
msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include

DEFINES += HAVE_STDINT_H
DEFINES += HAVE_CONFIG_H

HEADERS += \
	# MeCaSearch.h \
	AbstractSimpleIO.h \
	MeCaSearch.h \
	config.h \
	mecab/mecab/src/char_property.h \
	mecab/mecab/src/common.h \
	mecab/mecab/src/connector.h \
	mecab/mecab/src/context_id.h \
	mecab/mecab/src/darts.h \
	mecab/mecab/src/dictionary.h \
	mecab/mecab/src/dictionary_rewriter.h \
	mecab/mecab/src/feature_index.h \
	mecab/mecab/src/freelist.h \
	mecab/mecab/src/iconv_utils.h \
	mecab/mecab/src/lbfgs.h \
	mecab/mecab/src/learner_node.h \
	mecab/mecab/src/learner_tagger.h \
	mecab/mecab/src/mecab.h \
	mecab/mecab/src/mmap.h \
	mecab/mecab/src/nbest_generator.h \
	mecab/mecab/src/param.h \
	mecab/mecab/src/scoped_ptr.h \
	mecab/mecab/src/stream_wrapper.h \
	mecab/mecab/src/string_buffer.h \
	mecab/mecab/src/thread.h \
	mecab/mecab/src/tokenizer.h \
	mecab/mecab/src/ucs.h \
	mecab/mecab/src/ucstable.h \
	mecab/mecab/src/utils.h \
	mecab/mecab/src/viterbi.h \
	mecab/mecab/src/winmain.h \
	mecab/mecab/src/writer.h

SOURCES += \
	# MeCaSearch.cpp \
	AbstractSimpleIO.cpp \
	MeCaSearch.cpp \
	gzip.cpp \
	libmain.cpp \
	mecab/mecab/src/char_property.cpp \
	mecab/mecab/src/connector.cpp \
	mecab/mecab/src/context_id.cpp \
	mecab/mecab/src/dictionary.cpp \
	mecab/mecab/src/dictionary_compiler.cpp \
	mecab/mecab/src/dictionary_generator.cpp \
	mecab/mecab/src/dictionary_rewriter.cpp \
	mecab/mecab/src/eval.cpp \
	mecab/mecab/src/feature_index.cpp \
	mecab/mecab/src/iconv_utils.cpp \
	mecab/mecab/src/lbfgs.cpp \
	mecab/mecab/src/learner.cpp \
	mecab/mecab/src/learner_tagger.cpp \
	mecab/mecab/src/libmecab.cpp \
	mecab/mecab/src/nbest_generator.cpp \
	mecab/mecab/src/param.cpp \
	mecab/mecab/src/string_buffer.cpp \
	mecab/mecab/src/tagger.cpp \
	mecab/mecab/src/tokenizer.cpp \
	mecab/mecab/src/utils.cpp \
	mecab/mecab/src/viterbi.cpp \
	mecab/mecab/src/writer.cpp \
	xxd_charbin_gz.c \
	xxd_dicrc_gz.c \
	xxd_matrixbin_gz.c \
	xxd_sysdic_gz.c \
	xxd_unkdic_gz.c
