#!/bin/bash

: "${QMAKE:=qmake6}"

pushd file
autoreconf -if
./configure
make -j10
popd

cp file/src/magic.h gen/
cp file/magic/magic.mgc gen/
pushd gen
#gzip -k -f magic.mgc
#xxd -i magic.mgc.gz >magic_mgc_gz.c
zstd -19 magic.mgc -f -o magic.mgc.zst
xxd -i magic.mgc.zst >xxd_magic_mgc_zst.c
popd
