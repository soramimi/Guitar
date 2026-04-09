call c:\vcvars\vcvars.bat
rmdir /s /q _build
mkdir _build
cd _build
C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe "CONFIG+=release" ..\libmecasearch.pro
..\..\misc\jom.exe
