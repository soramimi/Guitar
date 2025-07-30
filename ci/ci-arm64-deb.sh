#!/bin/bash

ruby prepare.rb

pushd filetype
bash build-gcc.sh
popd

rm -fr _build_guitar
mkdir _build_guitar
pushd _build_guitar
qmake "CONFIG+=release" ../Guitar.pro
make -j8
popd

pushd packaging/deb
ruby mk-deb.rb
file=`./debname.rb`
curl -T $file ftp://10.168.0.5/Public/pub/nightlybuild/
popd

