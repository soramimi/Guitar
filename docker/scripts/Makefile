main:

clean:
	cd /Guitar && rm -fr _bin _build
	cd /Guitar/filetype && rm -fr _bin _build

build: clean
	cd /Guitar ; ruby prepare.rb
	cd /Guitar/filetype/ && bash build-gcc.sh
	mkdir /Guitar/_build && cd /Guitar/_build && qmake6 "CONFIG+=release" /Guitar/Guitar.pro && make -j8

build5: clean
	cd /Guitar ; ruby prepare.rb
	cd /Guitar/filetype/ && QMAKE=qmake bash build-gcc.sh
	mkdir /Guitar/_build && cd /Guitar/_build && qmake6 "CONFIG+=release" /Guitar/Guitar.pro && make -j8

deb: build
	cd /Guitar/packaging/deb && ruby mk-deb.rb

appimage: build5
	bash make-appimage.sh

doxy:
	cd /Guitar/doxy ; make clean ; make

migemo:
	cd /Guitar/cmigemo ; ./configure
	cd /Guitar/cmigemo ; make gcc
	cd /Guitar/cmigemo/dict ; make utf-8

