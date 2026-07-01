main:

clean:
	cd /Guitar && rm -fr _bin _build
	cd /Guitar/filetype && rm -fr _bin _build

build: clean filetypeplugin incrementalsearchplugin
	cd /Guitar ; ruby prepare.rb
	mkdir /Guitar/_build && cd /Guitar/_build && qmake6 "CONFIG+=release" /Guitar/Guitar.pro && make -j8

build5: clean filetypeplugin5 incrementalsearchplugin5
	cd /Guitar ; ruby prepare.rb
	mkdir /Guitar/_build && cd /Guitar/_build && qmake "CONFIG+=release" /Guitar/Guitar.pro && make -j8

filetypeplugin:
	cd /Guitar/subprojects/FileTypePlugin/ && bash build-gcc.sh

filetypeplugin5:
	cd /Guitar/subprojects/FileTypePlugin/ && QMAKE=qmake bash build-gcc.sh

incrementalsearchplugin:
	cd /Guitar/subprojects/IncrementalSearchPlugin/ && bash build-gcc.sh

incrementalsearchplugin5:
	cd /Guitar/subprojects/IncrementalSearchPlugin/ && QMAKE=qmake bash build-gcc.sh

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

