QT=~/Qt/6.7.2/macos

mkdir _bin

pushd filetype
bash build-gcc.sh
popd

pushd packaging/mac
rm -fr Guitar.app
rm -fr build
mkdir build

pushd build
${QT}/bin/qmake "CONFIG+=release" ../../../Guitar.pri
make -j16
${QT}/bin/macdeployqt Guitar.app
popd

mv build/Guitar.app .
rm -f Guitar-macos.zip
zip -r Guitar-macos.zip Guitar.app
curl -T Guitar-macos.zip ftp://10.168.0.5/Public/pub/nightlybuild/
popd

