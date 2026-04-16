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

make1 file
make1 oniguruma

make2 libfile release
make2 libfile debug
make2 liboniguruma release
make2 liboniguruma debug
make2 libfiletype release
make2 libfiletype debug

