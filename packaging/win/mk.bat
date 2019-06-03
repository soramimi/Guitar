rd /s /q packages\jp.soramimi.guitar\data
rem 7z x ../../_release/Guitar-1.0.0-win32.zip -opackages\jp.soramimi.guitar
7z x ../../_release/%1 -opackages\jp.soramimi.guitar
ren packages\jp.soramimi.guitar\Guitar data
C:\Qt\Tools\QtInstallerFramework\3.1\bin\binarycreator.exe -c config/config.xml -p packages GuitarSetup.exe 
