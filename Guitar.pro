include(Guitar.pri)
CONFIG(debug,debug|release):TARGET = Guitard
CONFIG(release,debug|release):TARGET = Guitar
DESTDIR=$$PWD/_bin
