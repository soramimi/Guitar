
QMAKE = C:\Qt\6.5.2\msvc2019_64\bin\qmake.exe
WINDEPLOYQT = C:\Qt\6.5.2\msvc2019_64\bin\windeployqt.exe
JOM = C:\Qt\Tools\QtCreator\bin\jom.exe
7Z = "C:\Program Files\7-Zip\7z.exe"

all:
	-rmdir /s /q ..\..\_bin
	-rmdir /s /q _build
	mkdir ..\..\_bin
	mkdir _build
# filetype
	mkdir _build\filetype
	copy ..\..\filetype.pro _build
	xcopy /s /y ..\..\filetype _build\filetype
	cd _build && $(QMAKE) && $(JOM)
# zlib
	mkdir _build\zlib
	cd _build && git clone https://github.com/madler/zlib.git
	copy ..\..\zlib.pro _build\zlib
	cd _build\zlib && $(QMAKE) zlib.pro && $(JOM)
	copy _build\zlib\_bin\libz.lib _build\_bin
# Guitar
	copy _build\_bin\* ..\..\_bin
	mkdir _build\guitar
	cd _build\guitar && $(QMAKE) ..\..\..\..\Guitar.pro && $(JOM)
	mkdir _release
	copy ..\..\_bin\Guitar.exe _release
	$(WINDEPLOYQT) _release\Guitar.exe
	$(7Z) x -o_build ..\..\misc\win32tools.zip
	copy _build\win32tools\*.* _release
