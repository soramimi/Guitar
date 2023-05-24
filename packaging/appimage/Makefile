
NPROC := $(shell nproc)

all: build appimage

appimage:
	mkdir -p _appdir/usr/bin
	mkdir -p _appdir/usr/share/applications
	mkdir -p _appdir/usr/share/icons/hicolor/256x256/apps
	mkdir -p _appdir/usr/share/icons/hicolor/scalable/apps

	cp _build/Guitar _appdir/usr/bin/Guitar
	strip _appdir/usr/bin/Guitar
	cp ../../LinuxDesktop/Guitar.svg _appdir/usr/share/icons/hicolor/scalable/apps/Guitar.svg
	cp ../../src/resources/image/guitar.png _appdir/usr/share/icons/hicolor/256x256/apps/Guitar.png
	cp Guitar.desktop _appdir/Guitar.desktop

	./linuxdeploy-x86_64.AppImage --appdir _appdir -i _appdir/usr/share/icons/hicolor/scalable/apps/Guitar.svg -d _appdir/Guitar.desktop --output appimage

	mv Guitar-*.AppImage Guitar-x86_64.AppImage

clean:
	rm -f Guitar-*.AppImage
	rm -fr _appdir
	rm -fr _build

build:
	-mkdir _build
	cd _build; /opt/qt6static/bin/qmake6 "CONFIG+=release" ../../../Guitar.pri; make -j$(NPROC)

archlinux:
	sudo pacman -S --noconfirm base-devel double-conversion libb2 ruby
	sudo pacman -S --noconfirm libxcb libxkbcommon-x11 xcb-util-cursor xcb-util-wm xcb-util-keysyms libsm libjpeg-turbo libdrm mtdev tslib libinput libx11 libglvnd libpng harfbuzz md4c freetype2 fontconfig fuse2

ubuntu:
	sudo apt install -y ruby libmagic-dev libssl-dev
	sudo apt install -y libfontconfig1-dev libfreetype6-dev libx11-dev libx11-xcb-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev libxcb1-dev libxcb-cursor-dev libxcb-glx0-dev libxcb-keysyms1-dev libxcb-image0-dev libxcb-shm0-dev libxcb-icccm4-dev libxcb-sync-dev libxcb-xfixes0-dev libxcb-shape0-dev libxcb-randr0-dev libxcb-render-util0-dev libxcb-util-dev libxcb-xinerama0-dev libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev
