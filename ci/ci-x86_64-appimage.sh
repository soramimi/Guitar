#!/bin/bash
pushd 'dirname $0'/..

pushd docker
make clean guitar-appimage
popd
curl -T docker/home/Guitar-x86_64.AppImage ftp://192.168.0.5/Public/pub/nightlybuild/

popd
