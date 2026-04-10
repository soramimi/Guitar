function newxxd () {
	gzip -k ../mecab/mecab-ipadic/$2 -c >/tmp/$1.gz
	perl newxxd.pl -n xxd_$1_gz /tmp/$1.gz >../xxd_$1_gz.c
}

newxxd dicrc dicrc
newxxd unkdic unk.dic
newxxd charbin char.bin
newxxd sysdic sys.dic
newxxd matrixbin matrix.bin

