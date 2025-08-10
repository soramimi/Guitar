#!/bin/bash

: "${QMAKE:=qmake6}"

pushd file
autoreconf -if
./configure
make -j10
popd

pushd oniguruma
autoreconf -if
./configure
make -j10
popd

echo --- libfile release
rm -fr _build
mkdir _build
pushd _build
${QMAKE} "CONFIG+=release" ../libfile.pro
make -j10
popd

echo --- libfile debug
rm -fr _build
mkdir _build
pushd _build
${QMAKE} "CONFIG+=debug" ../libfile.pro
make -j10
popd

echo --- liboniguruma release
rm -fr _build
mkdir _build
pushd _build
${QMAKE} "CONFIG+=release" ../liboniguruma.pro
make -j10
popd

echo --- liboniguruma debug
rm -fr _build
mkdir _build
pushd _build
${QMAKE} "CONFIG+=debug" ../liboniguruma.pro
make -j10
popd

echo --- libfiletype release
rm -fr _build
mkdir _build
pushd _build
${QMAKE} "CONFIG+=release" ../libfiletype.pro
make -j10
popd

echo --- libfiletype debug
rm -fr _build
mkdir _build
pushd _build
${QMAKE} "CONFIG+=debug" ../libfiletype.pro
make -j10
popd

