main:

build:
	cd /Guitar ; ruby prepare.rb
	rm -fr _build
	mkdir _build
	cd _build ; qmake "CONFIG+=release" /Guitar/Guitar.pri ; make -j8

deb: build
	ruby make-deb.rb

appimage: build
	bash make-appimage.sh

doxy:
	cd /Guitar/doxy ; make clean ; make
