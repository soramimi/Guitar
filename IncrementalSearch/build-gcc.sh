#!/bin/bash

# このスクリプト内の相対パスが安定するよう、リポジトリのルートへ移動する。
BASEDIR=$(realpath $(dirname $0))
pushd $BASEDIR

# 環境変数で qmake を上書きできるようにし、未指定時は qmake6 を使う。
: "${QMAKE:=qmake6}"

# UTF-8 の MeCab 辞書を組み立てる作業用ディレクトリ。

DICDIR=utf8dic

function reset_utf8_dic () {
	# 元の ipadic CSV ファイルを EUC-JP から UTF-8 に変換する。
	for file in mecab/mecab-ipadic/*.csv; do
		name=`basename $file`
	echo $name
	iconv -f EUC-jp -t UTF-8 $file -o ${DICDIR}/$name
	done
}

# 元の辞書ファイルから作業用ディレクトリを作り直す。
rm -fr ${DICDIR}
mkdir ${DICDIR}
reset_utf8_dic

# ユーザ辞書を作業用ディレクトリにコピーする。
cp userdic/* utf8dic/

# MeCab の辞書ファイルを作業用ディレクトリにコピーする。
cp mecab/mecab-ipadic/dicrc ${DICDIR}/
cp mecab/mecab-ipadic/*.def ${DICDIR}/
cp mecab/mecab-jumandic/model.def ${DICDIR}/

# UTF-8 の MeCab バイナリ辞書を生成する。
pushd ${DICDIR}
/usr/lib/mecab/mecab-dict-index -c UTF-8 -f UTF-8
popd

# 生成した辞書データを圧縮し、埋め込み用の C 配列へ変換する。

function my_xxd () {
        gzip -k ${DICDIR}/$2 -c >/tmp/$1.gz
        perl myxxd.pl -n xxd_$1_gz /tmp/$1.gz >xxd_$1_gz.c
}

my_xxd dicrc dicrc
my_xxd unkdic unk.dic
my_xxd charbin char.bin
my_xxd sysdic sys.dic
my_xxd matrixbin matrix.bin

# 同梱している MeCab ライブラリをソースからビルドする。

pushd mecab/mecab
./configure
make -j8
popd

# qmake と make で本体をビルドする。
rm -fr build
mkdir build
pushd build
${QMAKE} "CONFIG+=release" ../libmecasearch.pro
make -j8
popd

popd

