# How to build the Guitar (en) (draft)

# 0. Prerequirements

## 0.1 Compiler

__Windows__

	Microsoft Visual C++ 2013 or later
	Ruby : https://rubyinstaller.org/
	Perl : https://www.activestate.com/activeperl

__mac OS__

	Xcode
	Ruby
	Perl

__Linux (Ubuntu)__

	GCC
	Ruby: sudo apt-get install ruby
	Perl: sudo apt-get install perl

## 0.2 Dependent libraries

Qt : https://www.qt.io/download-open-source/#section-2
- Please download and install Qt SDK suitable for your platform.

zlib : https://github.com/madler/zlib.git

OpenSSL : https://www.openssl.org/source/ | https://github.com/openssl/openssl.git
- required openssl-1.0.x
- openssl-1.1.x is not supported

## 0.3 Download sources

- Guitar

	`(https) git clone https://github.com/soramimi/Guitar.git`
	`(ssh) git clone git@github.com:soramimi/Guitar.git`

- zlib

	`git clone https://github.com/madler/zlib.git`

- OpenSSL

	`git clone https://github.com/openssl/openssl.git`
	`...or download source from https://www.openssl.org/source/`

## 0.4 Directories (example)

	/home/my/work/Guitar
	/home/my/work/openssl
	/home/my/work/zlib

# 1. Make the zlib

Start the Qt Creator and open Guitar/zlib.pro

- Set build directory to:

- (ex. for Windows) `C:\work\_build_zlib`

- (ex. for Linux/mac) `/home/my/work/_build_zlib_Debug|Release`

Run [Build] menu (or press Ctrl+B)


---

__Make OpenSSL on Windows__

please read the INSTALL.W32

recommended to install the OpenSSL on `C:\openssl`

	perl Configure VC-WIN32 --prefix=c:\openssl


__Make OpenSSL on macOS__

	$ cd openssl
	$ git checkout -b OpenSSL_1_0_2-stable origin/OpenSSL_1_0_2-stable 
	$ ./Configure darwin64-x86_64-cc --prefix=/usr/local 
	$ make -j4 
	$ sudo make install


__Install OpenSSL on Linux__

	$ sudo apt-get install libssl-dev


# 1. Make the Guitar

first, run the script 'prepare.rb'

	$ cd Guitar
	$ ruby prepare.rb

start the Qt Creator, open the project file 'Guitar.pro'
- Set build directory to:

	(ex. for Windows) `C:\work\_build_Guitar`
	
	(ex. for Linux/mac) `/home/my/work/_build_Guitar_Debug|Release`

- do [Run qmake] (in [Build] menu)
- do [Build] (or press Ctrl+B)


# Note

(on Windows) necessary to copy OpenSSL's 'libeay32.dll' and 'ssleay32.dll' to the same folder as 'Guitar.exe'.
