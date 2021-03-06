CMAKE_MINOR=3.5
CMAKE=cmake-${CMAKE_MINOR}.2-Linux-x86_64

sudo apt-get --assume-yes update
sudo apt-get --assume-yes install ragel valgrind
wget https://cmake.org/files/v${CMAKE_MINOR}/${CMAKE}.tar.gz
tar -xzf ${CMAKE}.tar.gz
mv ${CMAKE} cmake-build
./cmake-build/bin/cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
./cmake-build/bin/cmake --build build
