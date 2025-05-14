rm -fr ../../_bin/Guitar.app
rm -fr build
mkdir build
cd build
/opt/Qt/6.2.4/macos/bin/qmake ../../../Guitar.pro
make -j10
cd ..
rm -fr Guitar.app
mv ../../_bin/Guitar.app .
/opt/Qt/6.2.4/macos/bin/macdeployqt Guitar.app
rm Guitar-macos.zip
zip -r Guitar-macos.zip Guitar.app
curl -T Guitar-macos.zip ftp://192.168.0.5/Public/pub/nightlybuild/
