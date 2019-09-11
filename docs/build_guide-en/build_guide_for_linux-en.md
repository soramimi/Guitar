# Build the Guitar(en) on Linux

Tested on Ubuntu19.04, Archlinux.

# 0. Prepare:
## 0.1. Dependencies:

- GCC:

- Ruby:

- Perl:

- Openssl:
required openssl-1.0.x, openssl-1.1.x is not supported

- zlib: 

__Install Dependencies__

- Ubuntu:

	`sudo apt install ruby perl libssl-dev zlib1g-dev`

- Archlinux:

	`sudo pacman -S ruby perl openssl zlib`

## 0.3 Download sources

	git clone git@github.com:soramimi/Guitar.git

## 1. Build:
## 1.1 Build with cmake:

	cd $(YOUR_DIR)/Guitar
	ruby prepare.rb
	mkdir build
	cd build
	cmake ..
	make -j(nproc)

## 1.2 ~~Build with qmake:~~

Notice: seems something wrong in `.pro` file

	cd $(YOUR_DIR)/Guitar
	ruby prepare.rb
	mkdir build
	cd build
	qmake ..
	make -j(nproc)

## 1.3 Build with qtcreator:

- start the QtCreator, open the project file `Guitar.pro`
- do [Run qmake]
- do [Build]

__notice:__

knowen issue: `can not find Guitar/_bin/libzd.a` when building

solution: edit `Guitar.pro` line 73 to line 77.

origin:

	!haiku {
		unix:CONFIG(debug, debug|release):LIBS += $$PWD/_bin/libzd.a
		unix:CONFIG(release, debug|release):LIBS += $$PWD/_bin/libz.a
		#unix:LIBS += -lz
	}

To:

	!haiku {
		#unix:CONFIG(debug, debug|release):LIBS += $$PWD/_bin/libzd.a
		#unix:CONFIG(release, debug|release):LIBS += $$PWD/_bin/libz.a
		unix:LIBS += -lz
	}
