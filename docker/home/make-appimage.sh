#!/bin/bash
export QMAKE=/usr/bin/qmake6

rm -f Guitar-*.AppImage
mkdir -p _appdir/usr/bin
mkdir -p _appdir/usr/lib/soramimi.jp
mkdir -p _appdir/usr/share/applications
mkdir -p _appdir/usr/share/icons/hicolor/256x256/apps
mkdir -p _appdir/usr/share/icons/hicolor/scalable/apps

cp /Guitar/_bin/Guitar _appdir/usr/bin/
cp /Guitar/lib/soramimi.jp/libfiletypeplugin.so _appdir/usr/lib/soramimi.jp/
cp /Guitar/lib/soramimi.jp/libincrementalsearchplugin.so _appdir/usr/lib/soramimi.jp/
strip _appdir/usr/bin/Guitar
strip _appdir/usr/lib/soramimi.jp/libfiletypeplugin.so
strip _appdir/usr/lib/soramimi.jp/libincrementalsearchplugin.so
cp /Guitar/LinuxDesktop/Guitar.svg _appdir/usr/share/icons/hicolor/scalable/apps/Guitar.svg
cp /Guitar/src/resources/image/guitar.png _appdir/usr/share/icons/hicolor/256x256/apps/Guitar.png
cp /Guitar/packaging/appimage/Guitar.desktop _appdir/Guitar.desktop
/Guitar/packaging/appimage/linuxdeploy-x86_64.AppImage --plugin qt --appdir _appdir -i _appdir/usr/share/icons/hicolor/scalable/apps/Guitar.svg -d _appdir/Guitar.desktop --output appimage

