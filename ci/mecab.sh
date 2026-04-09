#!/bin/bash

pushd IncrementalSearch/mecab/mecab
./configure
make
cd ../..
./makedic.sh
cd scripts
./gen.sh
popd

