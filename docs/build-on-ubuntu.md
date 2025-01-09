sudo apt install build-essential ruby qmake6 libqt6core6 libqt6gui6 libqt6svg6-dev libqt6core5compat6-dev zlib1g-dev libgl1-mesa-dev libssl-dev

git clone https://github.com/soramimi/Guitar.git

cd Guitar

ruby prepare.rb

mkdir build

cd build

qmake6 ../Guitar.pro

make -j8

