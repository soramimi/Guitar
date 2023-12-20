rm _bin -fr
rm _build -fr
mkdir _bin
ruby prepare.rb
cd packaging/deb
mkdir _build
cd _build
qmake "CONFIG+=release" ../../../Guitar.pri
make -j2
cd ..
cp _build/Guitar .
ruby mk-deb.rb
file=`./debname.rb`
curl -T $file ftp://192.168.0.5/Public/pub/nightlybuild/

