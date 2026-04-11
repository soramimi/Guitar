
#!/bin/bash
cd /Guitar ; ruby prepare.rb
cd /Guitar/filetype/ && QMAKE=qmake bash build-gcc.sh
cd /Guitar/IncrementalSearch/ && QMAKE=${QMAKE} bash build-gcc.sh
rm -fr /Guiltar/_build2 && mkdir /Guitar/_build2 && cd /Guitar/_build2 && ${QMAKE} "CONFIG+=release" /Guitar/Guitar.pro && make -j8

