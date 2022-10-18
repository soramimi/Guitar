rm _bin -fr
rm _build -fr
mkdir _bin
ruby prepare.rb
cp /usr/lib/aarch64-linux-gnu/libz.a _bin
cd packaging/deb
mkdir _build
cd _build
qmake ../../../Guitar.pro
make -j2
cd ..
cp ../../_bin/Guitar .
ruby mk-deb.rb
file=`./debname.rb`
curl -T $file ftp://10.10.10.5/Public/pub/nightlybuild/

