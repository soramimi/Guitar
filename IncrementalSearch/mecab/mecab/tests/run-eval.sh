#!/bin/sh

#
cd eval
../../src/mecab-system-eval --level="0 1 2 3 4" system answer > test.out
diff test.gld test.out;
if [ "$?" != "0" ]
then
  echo "runtests faild in $dir"
  exit -1
fi;
rm -f test.out
exit 0
