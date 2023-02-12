#cmake . -D CMAKE_TOOLCHAIN_FILE=./cmake/flags-gcc-x86_64.cmake -D QT=ON  --preset regular
#export LDFLAGS="-L/usr/lib -static-libstdc++ -lswitchres"
#cmake . -D CMAKE_TOOLCHAIN_FILE=./cmake/flags-gcc-x86_64.cmake -D QT=OFF -D WIN32=ON --preset regular
cmake . -D CMAKE_TOOLCHAIN_FILE=./cmake/flags-gcc-x86_64.cmake -D QT=OFF -D WIN32=ON --preset regular
ninja

