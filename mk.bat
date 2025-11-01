@echo off
cd %~dp0\make
call vcvars64.bat
rem ..\misc\jom.exe /J 1 /L /S OS=windows ARCH=x86_64 /F Makefile %1
nmake OS=windows ARCH=x86_64 /F Makefile %1
cd %~dp0
