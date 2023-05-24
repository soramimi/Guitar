mkdir _bin
cp /Users/soramimi/develop/Guitar/_bin/libz.a _bin/
cd packaging/mac
rm -fr build
mkdir build
cd build
/opt/Qt/5.15.0/clang_64/bin/qmake "CONFIG+=release" ../../../Guitar.pri
make -j10
cd ..
rm -fr Guitar.app
mv build/Guitar.app .
/opt/Qt/5.15.0/clang_64/bin/macdeployqt Guitar.app
rm -f Guitar-macos.zip
zip -r Guitar-macos.zip Guitar.app
curl -T Guitar-macos.zip ftp://10.10.10.5/Public/pub/nightlybuild/

