#!/bin/bash
cd ~
rm -fr _guitar-nightly-ci
mkdir _guitar-nightly-ci
pushd _guitar-nightly-ci
git clone git@github.com:soramimi/Guitar.git
cd Guitar/docker
make build
#make guitar-deb
#make guitar-appimage
make guitar-doxy
popd

curl -T _guitar-nightly-ci/Guitar/docker/home/guitar_*.deb ftp://192.168.0.5/Public/pub/nightlybuild/test/
curl -T _guitar-nightly-ci/Guitar/docker/home/Guitar-*.AppImage ftp://192.168.0.5/Public/pub/nightlybuild/test/
curl -T _guitar-nightly-ci/Guitar/doxy/guitar-doxy-html.tar.zst ftp://192.168.0.5/Public/pub/nightlybuild/
