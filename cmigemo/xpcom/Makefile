# Copyright 2005 MURAOKA Taro(KoRoN)/KaoriYa

GECKO_SDKDIR 	= D:\software\gecko-sdk
MIGEMO_DIR	= ..

IDLNAME		= nsIMigemo
SRC		= nsIMigemoModule.cpp nsMigemo.cpp
SRC_MIGEMO	= $(MIGEMO_DIR)\src\charset.c $(MIGEMO_DIR)\src\filename.c \
		  $(MIGEMO_DIR)\src\migemo.c $(MIGEMO_DIR)\src\mnode.c \
		  $(MIGEMO_DIR)\src\romaji.c $(MIGEMO_DIR)\src\rxgen.c \
		  $(MIGEMO_DIR)\src\wordbuf.c $(MIGEMO_DIR)\src\wordlist.c
OBJ_MIGEMO	= charset.obj filename.obj migemo.obj mnode.obj romaji.obj \
		  rxgen.obj wordbuf.obj wordlist.obj
OBJ		= $(SRC:.cpp=.obj) $(OBJ_MIGEMO)

INC		= -I $(GECKO_SDKDIR)\include -I $(MIGEMO_DIR)\src
DEF		= -D NDEBUG -D WIN32 -D _WINDOWS -D _MBCS -D _USRDLL \
		  -D XPCOM_GLUE
LIBS		= nspr4.lib plc4.lib plds4.lib xpcom.lib xpcomglue.lib

CFLAGS		= -nologo $(INC) $(DEF) -MD -W3 -O1
CPPFLAGS	= $(CFLAGS)
LD		= link.exe
LDFLAGS		= -nologo -machine:I386 -libpath:$(GECKO_SDKDIR)\lib
XPIDL		= $(GECKO_SDKDIR)\bin\xpidl
XPIDL_FLAGS	= -I $(GECKO_SDKDIR)\idl

target: $(IDLNAME).xpt $(IDLNAME).dll

$(IDLNAME).dll: $(OBJ)
	$(LD) $(LDFLAGS) -dll -out:$@ -implib:$(IDLNAME).lib $(LIBS) $(OBJ)

$(IDLNAME).xpt: $(IDLNAME).idl
	$(XPIDL) -m typelib $(XPIDL_FLAGS) $(IDLNAME).idl

$(IDLNAME).h: $(IDLNAME).idl
	$(XPIDL) -m header $(XPIDL_FLAGS) $(IDLNAME).idl

clean:
	del /F $(IDLNAME).lib
	del /F $(IDLNAME).exp
	del /F *.obj

distclean: clean
	del /F $(IDLNAME).xpt
	del /F $(IDLNAME).h
	del /F $(IDLNAME).dll

include depend.mak

{$(MIGEMO_DIR)\src}.c.obj:
	$(CC) $(CFLAGS) -Fo$@ /c $<
