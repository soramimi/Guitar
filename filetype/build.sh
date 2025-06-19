#!/bin/bash

pushd file
autoreconf -i
./configure
make -j10
popd

rm -fr _build
mkdir _build
pushd _build
qmake6 "CONFIG+=release" ../libfiletype.pro
make -j10
popd

cp file/src/magic.h .
#./scripts/dumpc.pl file/magic/magic.mgc magic_mgc >lib/magic_mgc.c

