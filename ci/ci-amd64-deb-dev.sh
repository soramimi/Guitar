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
<<<<<<< HEAD
ruby mk-deb.rb
file=`./debname.rb`
curl -T $file ftp://10.0.0.5/Public/pub/_tmp/a.bin

=======
SUFFIX=-dev ruby mk-deb.rb
file=`SUFFIX=-dev ./debname.rb`
curl -T $file ftp://10.0.0.5/Public/pub/_tmp/
>>>>>>> 919d5e7064918feb5ad3fb5dcbc2ce515c6dcc02
