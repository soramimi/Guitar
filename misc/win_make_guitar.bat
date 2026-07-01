set DIR=%~dp0\..\
set BUILDDIR=%DIR%\_build\
cd %DIR%

if "%BINDIR%"=="" (
  set BINDIR=%DIR%\_bin\
)

if "%LIBDIR%"=="" (
  set LIBDIR=%DIR%\lib\
)

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

if "%WINDEPLOYQT%"=="" (
  set WINDEPLOYQT=C:\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe
)

if "%NO_DEBUG%"=="" (
  call :MAKE Guitar debug
)
if "%NO_RELEASE%"=="" (
  call :MAKE Guitar release
)

7z x -o%BUILDDIR% %DIR%\misc\win32tools.zip
copy %BUILDDIR%\win32tools\* %BINDIR% /Y

copy %LIBDIR%\*.dll %BINDIR% /Y

if "%NO_DEBUG%"=="" (
  call %WINDEPLOYQT% %BINDIR%\Guitard.exe
)
if "%NO_RELEASE%"=="" (
  call %WINDEPLOYQT% %BINDIR%\Guitar.exe
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

