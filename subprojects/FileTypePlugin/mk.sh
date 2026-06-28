#!/bin/sh

# make plugin module

rm _build1 -fr
mkdir _build1 2>/dev/null
cd _build1
QT_SELECT=5 qmake ../FileTypePlugin.pro
make -j4
cd ..

# make example application

rm _build2 -fr
mkdir _build2 2>/dev/null
cd _build2
QT_SELECT=5 qmake ../exampleapp.pro
make
cd ..
