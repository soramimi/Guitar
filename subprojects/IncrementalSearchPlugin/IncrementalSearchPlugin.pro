include(../../common.pri)

!win32:DESTDIR = $$PWD/lib
win32:DESTDIR = $$PWD/../../_bin
CONFIG(debug,debug|release):TARGET = incrementalsearchplugind
CONFIG(release,debug|release):TARGET = incrementalsearchplugin

TEMPLATE = lib
CONFIG += plugin
QT = core

gcc:QMAKE_CXXFLAGS += -include $$PWD/config.h

INCLUDEPATH += $$PWD/src
INCLUDEPATH += $$PWD/mecab/mecab/src

DEFINES += HAVE_STDINT_H
DEFINES += HAVE_CONFIG_H

DEFINES += USE_CUSTOM_DICTIONARY_LOADER

unix:LIBS += -ldl
win32:LIBS += -lole32 -loleaut32

win32 {
	LIBS += -lzlib
	LIBS += -lzstd
}

!win32 {
	LIBS += -lz
	LIBS += -lzstd
}


# CONFIG += use_cudachi
use_cudachi {
	DEFINES += USE_SUDACHI
	LIBS = -L$$DESTDIR -lsudachi_capi
	
	# CONFIG(debug,debug|release):LIBS += -L/home/soramimi/develop/sudachi-example/sudachi-capi/target/release -lsudachi_capi
	# CONFIG(release,debug|release):IBS += -L/home/soramimi/develop/sudachi-example/sudachi-capi/target/debug -lsudachi_capi
	
	# CONFIG(debug,debug|release):LIBS := -L../sudachi-capi/target/debug -lsudachi_capi -Wl,-rpath,'$$ORIGIN/../sudachi-capi/target/debug'
	# CONFIG(release,debug|release):LIBS := -L../sudachi-capi/target/release -lsudachi_capi -Wl,-rpath,'$$ORIGIN/../sudachi-capi/target/release'
}


HEADERS += \
	src/IncrementalSearch.h \
	src/IncrementalSearchInterface.h \
	src/IncrementalSearchPlugin.h \
	src/MyMecab.h \
	src/MySudachi.h \
	src/romaji.h \
	src/zs.h
SOURCES += \
	src/IncrementalSearch.cpp \
	src/IncrementalSearchInterface.cpp \
	src/IncrementalSearchPlugin.cpp \
	src/MyMecab.cpp \
	src/MySudachi.cpp \
	src/romaji.cpp \
	src/zs.cpp

HEADERS += \
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
	mecab/mecab/src/writer.h \
	src/AbstractSimpleIO.h

SOURCES += \
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
	src/AbstractSimpleIO.cpp \
	src/gzip.cpp \
	src/libmain.cpp
	
# SOURCES += \
# 	gen/xxd_charbin_gz.c \
# 	gen/xxd_dicrc_gz.c \
# 	gen/xxd_matrixbin_gz.c \
# 	gen/xxd_sysdic_gz.c \
# 	gen/xxd_unkdic_gz.c

SOURCES += \
	gen/xxd_charbin_zst.c \
	gen/xxd_dicrc_zst.c \
	gen/xxd_matrixbin_zst.c \
	gen/xxd_sysdic_zst.c \
	gen/xxd_unkdic_zst.c

DISTFILES += \
	incrementalsearchplugin.json

!win32 {
	LIBS += -ldl
}
