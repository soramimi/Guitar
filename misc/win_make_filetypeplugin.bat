set DIR=%~dp0\..\subprojects\FileTypePlugin
set BUILDDIR=%DIR%\_build\
cd %DIR%

if "%QMAKE%"=="" (
  set QMAKE=C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe
)

if "%JOM%"=="" (
  set JOM=C:\Qt\Tools\QtCreator\bin\jom\jom.exe
)

if "%VCVARS_BAT%"=="" (
  set VCVARS_BAT="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
call "%VCVARS_BAT%"

if "%NO_DEBUG%"=="" (
  call :MAKE libfile debug
  call :MAKE liboniguruma debug
  call :MAKE FileTypePlugin debug
)
if "%NO_RELEASE%"=="" (
  call :MAKE libfile release
  call :MAKE liboniguruma release
  call :MAKE FileTypePlugin release
)

exit /b 0

:MAKE
rmdir /s /q "%BUILDDIR%"
mkdir "%BUILDDIR%"
cd "%BUILDDIR%"
%QMAKE% "CONFIG+=%2" ../%1.pro
%JOM%
cd ..
exit /b 0

