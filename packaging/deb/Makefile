
QT_SELECT=5

all:
	-mkdir build 2>/dev/null
	cd ../..; ruby prepare.rb
	cd build; cmake ../../..
	cd build; make -j4
	cp build/Guitar .
	ruby mk-deb.rb

i386:
	-mkdir build 2>/dev/null
	cd ../..; ruby prepare.rb
	cd build; qmake ../../../Guitar-i386.pro
	cd build; make -j4
	cp ../../_bin/Guitar .
	ruby mk-deb.rb

amd64:
	-mkdir build 2>/dev/null
	cd ../..; ruby prepare.rb
	cd build; qmake ../../../Guitar-amd64.pro
	cd build; make -j4
	cp ../../_bin/Guitar .
	ruby mk-deb.rb

clean:
	-rm Guitar
	-rm -fr build
	-rm guitar_*.deb

