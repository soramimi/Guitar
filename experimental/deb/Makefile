
all:
	-mkdir build
	cd ../..; ruby prepare.rb
	cd build; cmake ../../..
	cd build; make -j8
	ruby mk-deb.rb

clean:
	-rm -fr build
	-rm guitar_*.deb

