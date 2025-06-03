#!/bin/bash

rm -f Guitar-*.AppImage
mkdir -p _appdir/usr/bin
mkdir -p _appdir/usr/share/applications
mkdir -p _appdir/usr/share/icons/hicolor/256x256/apps
mkdir -p _appdir/usr/share/icons/hicolor/scalable/apps

cp /Guitar/_bin/Guitar _appdir/usr/bin/Guitar
strip _appdir/usr/bin/Guitar
cp /Guitar/LinuxDesktop/Guitar.svg _appdir/usr/share/icons/hicolor/scalable/apps/Guitar.svg
cp /Guitar/src/resources/image/guitar.png _appdir/usr/share/icons/hicolor/256x256/apps/Guitar.png
cp /Guitar/packaging/appimage/Guitar.desktop _appdir/Guitar.desktop
/Guitar/packaging/appimage/linuxdeploy-x86_64.AppImage --appdir _appdir -i _appdir/usr/share/icons/hicolor/scalable/apps/Guitar.svg -d _appdir/Guitar.desktop --output appimage

