TARGET=MeCab
CSC=mcs
CS=mono
CXX=c++
PACKAGE=jp/ac/aist_nara/cl/mecab

LIBS=`mecab-config --libs`
INC=`mecab-config --cflags`

all:
	$(CXX) -O3 -c -fpic $(TARGET)_wrap.cxx  $(INC)
	$(CXX) -shared  $(TARGET)_wrap.o -o lib$(TARGET).so $(LIBS)
	$(CSC) -out:test.exe -codepage:utf8 *.cs

test:
	$(CS) MeCab.exe

clean:
cleanall:
