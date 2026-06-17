TARGET=MeCab
JAVAC=javac
JAVA=java
JAR=jar
CXX=c++
ifeq ($(OS),Windows_NT)
    #TBA
else
    OS:=$(shell uname -s)
    ifeq ($(OS),Linux)
        JAVA_HOME=/usr/lib/jvm/java-6-openjdk
        JNI_MD_FOLDER=linux
        LIB_TYPE=-shared
        LIB_EXT=so
        LD_PATH_VAR=LD_LIBRARY_PATH
    endif
    ifeq ($(OS),Darwin)
        JAVA_HOME=$(shell /usr/libexec/java_home)
        JNI_MD_FOLDER=darwin
        LIB_TYPE=-dynamiclib
        LIB_EXT=dylib
        LD_PATH_VAR=DYLD_LIBRARY_PATH
    endif
    #TBA
endif
INCLUDE=$(JAVA_HOME)/include

PACKAGE=org/chasen/mecab

LIBS=`mecab-config --libs`
INC=`mecab-config --cflags` -I$(INCLUDE) -I$(INCLUDE)/$(JNI_MD_FOLDER)

ifeq ($(OS),Darwin)
    LIBS:=-arch x86_64 $(LIBS)
    INC:=-arch x86_64 $(INC)
endif

all:
	$(CXX) -O3 -c -fpic $(TARGET)_wrap.cxx  $(INC)
	$(CXX) $(LIB_TYPE) $(TARGET)_wrap.o -o lib$(TARGET).$(LIB_EXT) $(LIBS)
	$(JAVAC) $(PACKAGE)/*.java
	$(JAVAC) test.java
	$(JAR) cfv $(TARGET).jar $(PACKAGE)/*.class

test:
	env $(LD_PATH_VAR)=. $(JAVA) test

clean:
	rm -fr *.jar *.o *.$(LIB_EXT) *.class $(PACKAGE)/*.class
	
cleanall:
	rm -fr $(TARGET).java *.cxx
