mkdir _bin
QT=~/Qt/6.7.0/macos
cd packaging/mac
rm -fr build
mkdir build
cd build
${QT}/bin/qmake "CONFIG+=release" ../../../Guitar.pri
make -j10
cd ..
rm -fr Guitar.app
mv build/Guitar.app .
${QT}/bin/macdeployqt Guitar.app
rm -f Guitar-macos.zip
zip -r Guitar-macos.zip Guitar.app
curl -T Guitar-macos.zip ftp://192.168.0.5/Public/pub/nightlybuild/

