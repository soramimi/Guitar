call c:\vcvars\vcvars.bat

rmdir /s /q _build
mkdir _build
cd _build
C:\Qt\6.10.0\msvc2022_64\bin\qmake.exe ../libfile.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..

rmdir /s /q _build
mkdir _build
cd _build
C:\Qt\6.10.0\msvc2022_64\bin\qmake.exe ../liboniguruma.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..

rmdir /s /q _build
mkdir _build
cd _build
C:\Qt\6.10.0\msvc2022_64\bin\qmake.exe ../libfiletype.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..

rem cd lib
rem lib /OUT:filetype.lib filetype.lib file.lib oniguruma.lib
rem del file.* oniguruma.*
rem cd ..


