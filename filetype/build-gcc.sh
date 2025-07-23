#!/bin/bash

: "${QMAKE:=qmake6}"

pushd file
autoreconf -if
./configure
make -j10
popd

#cp file/src/magic.h lib/
#cp file/magic/magic.mgc lib/
#pushd lib
#gzip -k -f magic.mgc
#xxd -i magic.mgc.gz >magic_mgc_gz.c
#popd

pushd oniguruma
autoreconf -if
./configure
make -j10
popd

rm -fr _build
mkdir _build
pushd _build
${QMAKE} "CONFIG+=release" ../libfile.pro
make -j10
popd

rm -fr _build
mkdir _build
pushd _build
${QMAKE} "CONFIG+=release" ../liboniguruma.pro
make -j10
popd

rm -fr _build
mkdir _build
pushd _build
${QMAKE} "CONFIG+=release" ../libfiletype.pro
make -j10
popd

#pushd lib
#ar x libfile.a
#ar x liboniguruma.a
#ar rs libfiletype.a *.o
#rm *.o libfile.a liboniguruma.a
#popd

#./scripts/dumpc.pl file/magic/magic.mgc magic_mgc >lib/magic_mgc.c

