rm -fr ../../_bin/Guitar.app
rm -fr build
mkdir build
cd build
/opt/Qt/5.12.3/clang_64/bin/qmake ../../../Guitar.pro
make -j10
cd ..
rm -fr Guitar.app
mv ../../_bin/Guitar.app .
/opt/Qt/5.12.3/clang_64/bin/macdeployqt Guitar.app
rm Guitar-macos.zip
zip -r Guitar-macos.zip Guitar.app
