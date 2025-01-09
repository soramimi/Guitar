# Microsoft Developer Studio Generated NMAKE File, Based on dll_migemo.dsp
!IF "$(CFG)" == ""
CFG=dll_migemo - Win32 Debug
!MESSAGE 構成が指定されていません。ﾃﾞﾌｫﾙﾄの dll_migemo - Win32 Debug を設定します。
!ENDIF 

!IF "$(CFG)" != "dll_migemo - Win32 Release" && "$(CFG)" != "dll_migemo - Win32 Debug"
!MESSAGE 指定された ﾋﾞﾙﾄﾞ ﾓｰﾄﾞ "$(CFG)" は正しくありません。
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "dll_migemo.mak" CFG="dll_migemo - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "dll_migemo - Win32 Release" ("Win32 (x86) Dynamic-Link Library" 用)
!MESSAGE "dll_migemo - Win32 Debug" ("Win32 (x86) Dynamic-Link Library" 用)
!MESSAGE 
!ERROR 無効な構成が指定されています。
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "dll_migemo - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\migemo.dll"


CLEAN :
	-@erase "$(INTDIR)\filename.obj"
	-@erase "$(INTDIR)\migemo.obj"
	-@erase "$(INTDIR)\migemo.res"
	-@erase "$(INTDIR)\mnode.obj"
	-@erase "$(INTDIR)\romaji.obj"
	-@erase "$(INTDIR)\rxgen.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\wordbuf.obj"
	-@erase "$(INTDIR)\wordlist.obj"
	-@erase "$(OUTDIR)\migemo.dll"
	-@erase "$(OUTDIR)\migemo.exp"
	-@erase "$(OUTDIR)\migemo.lib"
	-@erase "$(OUTDIR)\migemo.map"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /G6 /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DLL_MIGEMO_EXPORTS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x411 /fo"$(INTDIR)\migemo.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dll_migemo.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\migemo.pdb" /map:"$(INTDIR)\migemo.map" /machine:I386 /def:".\migemo.def" /out:"$(OUTDIR)\migemo.dll" /implib:"$(OUTDIR)\migemo.lib" 
DEF_FILE= \
	".\migemo.def"
LINK32_OBJS= \
	"$(INTDIR)\filename.obj" \
	"$(INTDIR)\migemo.obj" \
	"$(INTDIR)\mnode.obj" \
	"$(INTDIR)\romaji.obj" \
	"$(INTDIR)\rxgen.obj" \
	"$(INTDIR)\wordbuf.obj" \
	"$(INTDIR)\wordlist.obj" \
	"$(INTDIR)\migemo.res"

"$(OUTDIR)\migemo.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "dll_migemo - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\migemo.dll"


CLEAN :
	-@erase "$(INTDIR)\filename.obj"
	-@erase "$(INTDIR)\migemo.obj"
	-@erase "$(INTDIR)\migemo.res"
	-@erase "$(INTDIR)\mnode.obj"
	-@erase "$(INTDIR)\romaji.obj"
	-@erase "$(INTDIR)\rxgen.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\wordbuf.obj"
	-@erase "$(INTDIR)\wordlist.obj"
	-@erase "$(OUTDIR)\migemo.dll"
	-@erase "$(OUTDIR)\migemo.exp"
	-@erase "$(OUTDIR)\migemo.lib"
	-@erase "$(OUTDIR)\migemo.map"
	-@erase "$(OUTDIR)\migemo.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DLL_MIGEMO_EXPORTS" /Fp"$(INTDIR)\dll_migemo.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x411 /fo"$(INTDIR)\migemo.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dll_migemo.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\migemo.pdb" /map:"$(INTDIR)\migemo.map" /debug /machine:I386 /def:".\migemo.def" /out:"$(OUTDIR)\migemo.dll" /implib:"$(OUTDIR)\migemo.lib" /pdbtype:sept 
DEF_FILE= \
	".\migemo.def"
LINK32_OBJS= \
	"$(INTDIR)\filename.obj" \
	"$(INTDIR)\migemo.obj" \
	"$(INTDIR)\mnode.obj" \
	"$(INTDIR)\romaji.obj" \
	"$(INTDIR)\rxgen.obj" \
	"$(INTDIR)\wordbuf.obj" \
	"$(INTDIR)\wordlist.obj" \
	"$(INTDIR)\migemo.res"

"$(OUTDIR)\migemo.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("compile\dll_migemo.dep")
!INCLUDE "compile\dll_migemo.dep"
!ELSE 
!MESSAGE Warning: cannot find "compile\dll_migemo.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "dll_migemo - Win32 Release" || "$(CFG)" == "dll_migemo - Win32 Debug"
SOURCE=.\filename.c

"$(INTDIR)\filename.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\migemo.c

"$(INTDIR)\migemo.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\migemo.rc

"$(INTDIR)\migemo.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\mnode.c

"$(INTDIR)\mnode.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\romaji.c

"$(INTDIR)\romaji.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\rxgen.c

"$(INTDIR)\rxgen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\wordbuf.c

"$(INTDIR)\wordbuf.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\wordlist.c

"$(INTDIR)\wordlist.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

