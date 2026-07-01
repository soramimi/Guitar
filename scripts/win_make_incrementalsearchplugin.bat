@echo off
rem Build script for the IncrementalSearchPlugin subproject on Windows using MSVC and JOM.
rem Builds IncrementalSearchPlugin in both debug and release configurations unless
rem NO_DEBUG=1 or NO_RELEASE=1 is set.
rem
rem Environment variables (all optional -- defaults shown below):
rem   QMAKE       Path to qmake.exe
rem   JOM         Path to jom.exe (parallel nmake replacement)
rem   VCVARS_BAT  Path to vcvars64.bat (MSVC environment setup script)
rem   NO_DEBUG    Set to any non-empty value to skip the debug build
rem   NO_RELEASE  Set to any non-empty value to skip the release build

set DIR=%~dp0\..\subprojects\IncrementalSearchPlugin
set BUILDDIR=%DIR%\_build\
cd %DIR%

rem Use default Qt/MSVC paths if the environment variables are not set.
if "%QMAKE%"=="" (
  set QMAKE=C:\Qt\6.11.0\msvc2022_64\bin\qmake.exe
)

if "%JOM%"=="" (
  set JOM=C:\Qt\Tools\QtCreator\bin\jom\jom.exe
)

if "%VCVARS_BAT%"=="" (
  set VCVARS_BAT="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

rem Initialize the MSVC build environment.
call "%VCVARS_BAT%"
if errorlevel 1 exit /b 1

rem Build the debug configuration unless NO_DEBUG is set.
if "%NO_DEBUG%"=="" (
  call :MAKE IncrementalSearchPlugin debug
  if errorlevel 1 exit /b 1
)

rem Build the release configuration unless NO_RELEASE is set.
if "%NO_RELEASE%"=="" (
  call :MAKE IncrementalSearchPlugin release
  if errorlevel 1 exit /b 1
)

exit /b 0

rem Subroutine: clean the build directory, run qmake for the given project and
rem configuration, then build with JOM.
rem   %1 = project name (e.g. IncrementalSearchPlugin)
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

