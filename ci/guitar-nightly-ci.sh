#!/bin/bash
cd ~
rm -fr _guitar-nightly-ci
mkdir _guitar-nightly-ci
pushd _guitar-nightly-ci
git clone git@github.com:soramimi/Guitar.git
cd Guitar/docker
make build
make guitar-deb
make guitar-appimage
popd
#cp _guitar-nightly-ci/Guitar/docker/home/guitar_*.deb /mnt/lucy/pub/nightlybuild/test/
#cp _guitar-nightly-ci/Guitar/docker/home/Guitar-*.AppImage /mnt/lucy/pub/nightlybuild/test/
curl -T _guitar-nightly-ci/Guitar/docker/home/guitar_*.deb ftp://192.168.0.5/Public/pub/nightlybuild/test/
curl -T _guitar-nightly-ci/Guitar/docker/home/Guitar-*.AppImage ftp://192.168.0.5/Public/pub/nightlybuild/test/
