sudo apt install ruby qmake6 libqt6core6 libqt6gui6 libqt6svg6-dev libqt6core5compat6-dev libgl1-mesa-dev libssl-dev

git clone https://github.com/soramimi/Guitar.git

cd Guitar

ruby prepare.rb

mkdir build

cd build

qmake ../Guitar.pro

make -j8

