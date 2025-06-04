#!/bin/bash

ruby prepare.rb

rm -fr _build_libfiletype
mkdir _build_libfiletype
pushd _build_libfiletype
qmake "CONFIG+=release" ../filetype/libfiletype.pro
make -j8
popd

rm -fr _build_guitar
mkdir _build_guitar
pushd _build_guitar
qmake "CONFIG+=release" ../Guitar.pro
make -j8
popd

pushd packaging/deb
cp ../../_bin/Guitar .
export SUFFIX=-dev
ruby mk-deb.rb
file=`./debname.rb`
curl -T $file ftp://192.168.0.5/Public/pub/nightlybuild/
popd

