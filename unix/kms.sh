#sudo pacman -Sy base-devel cmake extra-cmake-modules pkg-config ninja libfreetype sdl2 libpng lib32-openal rtmidi faudio qt5-base qt5-xcb-private-headers qt5-tools libevdev vulkan-devel
ninja clean
cmake . -D CMAKE_TOOLCHAIN_FILE=./cmake/flags-gcc-x86_64.cmake -D QT=OFF -D WIN32=OFF --preset regular
ninja

