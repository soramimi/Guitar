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
date
ls
ping 10.10.10.5 -c 5
curl -T Guitar-macos.zip ftp://10.10.10.5/Public/pub/nightlybuild/
