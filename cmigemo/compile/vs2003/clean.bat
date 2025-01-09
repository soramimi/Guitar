@echo off
del /F /Q *.ncb
attrib -H *.suo
del /F /Q *.suo
rd /S /Q Debug
rd /S /Q Release

cd CMigemo
del /F /Q *.ncb
attrib -H *.suo
del /F /Q *.suo
rd /S /Q Debug
rd /S /Q Release
cd ..


cd MigemoDLL
del /F /Q *.ncb
attrib -H *.suo
del /F /Q *.suo
rd /S /Q Debug
rd /S /Q Release
cd ..
