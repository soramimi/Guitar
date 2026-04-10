#!/bin/bash
pushd mecab/mecab-ipadic
./configure --with-charset=utf8 --enable-utf8-only
make clean
make
popd
