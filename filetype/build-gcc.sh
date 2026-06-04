#!/bin/bash

: "${QMAKE:=qmake6}"

function make1 () {
	echo --- $1
	pushd $1
	autoreconf -if
	./configure
	make -j10
	popd
}

function make2 () {
	echo --- $1 $2
	rm -fr _build
	mkdir _build
	pushd _build
	${QMAKE} "CONFIG+=$2" ../$1.pro
	make -j10
	popd
}

# Note: fileのビルド時に以下のエラーが出る。magic等のビルドは必要だがfileコマンドは不要なので、このエラーは無視していい。
# file.c:371:13: error: ‘sandbox’ undeclared (first use in this function)
#  371 |         if (sandbox && enable_landlock(flags, action) == -1)
#      |             ^~~~~~~

make1 file
make1 oniguruma

make2 libfile release
make2 libfile debug
make2 liboniguruma release
make2 liboniguruma debug
make2 libfiletype release
make2 libfiletype debug

