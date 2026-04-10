#!/bin/bash
BASEDIR=$(realpath $(dirname $0))
pushd $BASEDIR

: "${QMAKE:=qmake6}"

pushd mecab/mecab
./configure
make
popd

./makedic.sh
pushd scripts
./gen.sh
popd

rm -fr build
mkdir build
pushd build
${QMAKE} "CONFIG+=release" ../libmecasearch.pro
make
popd

popd
