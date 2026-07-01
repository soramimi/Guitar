call c:\vcvars\vcvars.bat

echo --- libfile debug
rmdir /s /q _build
mkdir _build
cd _build
C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe "CONFIG+=debug" ../libfile.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..

echo --- libfile release
rmdir /s /q _build
mkdir _build
cd _build
C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe "CONFIG+=release" ../libfile.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..

echo --- liboniguruma debug
rmdir /s /q _build
mkdir _build
cd _build
C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe "CONFIG+=debug" ../liboniguruma.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..

echo --- liboniguruma release
rmdir /s /q _build
mkdir _build
cd _build
C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe "CONFIG+=release" ../liboniguruma.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..

echo --- FileTypePlugin debug
rmdir /s /q _build
mkdir _build
cd _build
C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe "CONFIG+=debug" ../FileTypePlugin.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..

echo --- FileTypePlugin release
rmdir /s /q _build
mkdir _build
cd _build
C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe "CONFIG+=release" ../FileTypePlugin.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..

