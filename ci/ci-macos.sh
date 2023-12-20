mkdir _bin
cp /Users/soramimi/develop/Guitar/_bin/libz.a _bin/
cd packaging/mac
rm -fr build
mkdir build
cd build
/opt/Qt/6.6.0/macos/bin/qmake "CONFIG+=release" ../../../Guitar.pri
make -j10
cd ..
rm -fr Guitar.app
mv build/Guitar.app .
/opt/Qt/6.6.0/macos/bin/macdeployqt Guitar.app
rm -f Guitar-macos.zip
zip -r Guitar-macos.zip Guitar.app
curl -T Guitar-macos.zip ftp://192.168.0.5/Public/pub/nightlybuild/

