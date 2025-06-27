#!/bin/bash

pushd `dirname $0`/..

pushd docker
make guitar-deb
popd

pushd packaging/deb
FILE=`./debname.rb`
ls -la $FILE
curl -T $FILE ftp://192.168.0.5/Public/pub/nightlybuild/
popd

popd

