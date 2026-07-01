@echo off
rem Build script for the main Guitar application on Windows using MSVC and JOM.
rem Builds Guitar in debug and/or release configurations, then runs windeployqt
rem to collect Qt runtime DLLs and copies additional DLLs from the lib directory.
rem
rem Environment variables (all optional — defaults shown below):
rem   BINDIR       Output directory for built executables (default: project\_bin\)
rem   LIBDIR       Directory containing extra DLLs to copy into BINDIR (default: project\lib\)
rem   QMAKE        Path to qmake.exe
rem   JOM          Path to jom.exe (parallel nmake replacement)
rem   VCVARS_BAT   Path to vcvars64.bat (MSVC environment setup script)
rem   WINDEPLOYQT  Path to windeployqt.exe
rem   NO_DEBUG     Set to any non-empty value to skip the debug build
rem   NO_RELEASE   Set to any non-empty value to skip the release build

set DIR=%~dp0\..\
set BUILDDIR=%DIR%\_build\
cd %DIR%

rem Use default output paths if not provided by the caller.
if "%BINDIR%"=="" (
  set BINDIR=%DIR%\_bin\
)

if "%LIBDIR%"=="" (
  set LIBDIR=%DIR%\lib\
)

rem Use default Qt/MSVC paths if the environment variables are not set.
if "%QMAKE%"=="" (
  set QMAKE="C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe"
)

if "%JOM%"=="" (
  set JOM="C:\Qt\Tools\QtCreator\bin\jom\jom.exe"
)

if "%WINDEPLOYQT%"=="" (
  set WINDEPLOYQT="C:\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe"
)

if "%VCVARS_BAT%"=="" (
  set VCVARS_BAT="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

rem Initialize the MSVC build environment.
call "%VCVARS_BAT%"
if errorlevel 1 exit /b 1

rem Build the debug configuration unless NO_DEBUG is set.
if "%NO_DEBUG%"=="" (
  call :MAKE Guitar debug
  if errorlevel 1 exit /b 1
)

rem Build the release configuration unless NO_RELEASE is set.
if "%NO_RELEASE%"=="" (
  call :MAKE Guitar release
  if errorlevel 1 exit /b 1
)

rem Extract bundled Windows helper tools (e.g. credential helpers) into BINDIR.
7z x -o%BUILDDIR% %DIR%\misc\win32tools.zip
if errorlevel 1 exit /b 1
copy %BUILDDIR%\win32tools\* %BINDIR% /Y
if errorlevel 1 exit /b 1

rem Copy plugin DLLs from the lib directory to BINDIR.
copy %LIBDIR%\*.dll %BINDIR% /Y
if errorlevel 1 exit /b 1

rem Run windeployqt to copy the required Qt runtime DLLs alongside each executable.
if "%NO_DEBUG%"=="" (
  call %WINDEPLOYQT% %BINDIR%\Guitard.exe
  if errorlevel 1 exit /b 1
)
if "%NO_RELEASE%"=="" (
  call %WINDEPLOYQT% %BINDIR%\Guitar.exe
  if errorlevel 1 exit /b 1
)

exit /b 0

rem Subroutine: clean the build directory, run qmake for the given project and
rem configuration, then build with JOM.
rem   %1 = project name (e.g. Guitar)
rem   %2 = configuration (debug or release)
:MAKE
rmdir /s /q "%BUILDDIR%"
mkdir "%BUILDDIR%"
cd "%BUILDDIR%"
%QMAKE% "CONFIG+=%2" ../%1.pro
if errorlevel 1 exit /b 1
%JOM%
if errorlevel 1 exit /b 1
cd ..
exit /b 0

