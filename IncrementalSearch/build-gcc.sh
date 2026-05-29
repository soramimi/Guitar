#!/bin/bash
BASEDIR=$(realpath $(dirname $0))
pushd $BASEDIR

: "${QMAKE:=qmake6}"

#

DICDIR=utf8dic

function reset_utf8_dic () {
	for file in mecab/mecab-ipadic/*.csv; do
		name=`basename $file`
	echo $name
	iconv -f EUC-jp -t UTF-8 $file -o ${DICDIR}/$name
	done
}

# reset_utf8_dic

pushd ${DICDIR}
make
popd


#


function newxxd () {
        gzip -k ${DICDIR}/$2 -c >/tmp/$1.gz
        perl myxxd.pl -n xxd_$1_gz /tmp/$1.gz >xxd_$1_gz.c
}

newxxd dicrc dicrc
newxxd unkdic unk.dic
newxxd charbin char.bin
newxxd sysdic sys.dic
newxxd matrixbin matrix.bin

#

pushd mecab/mecab
./configure
make -j8
popd

#./makedic.sh
#pushd scripts
#./gen.sh
#popd

rm -fr build
mkdir build
pushd build
${QMAKE} "CONFIG+=release" ../libmecasearch.pro
make -j8
popd

popd

