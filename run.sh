apt install mingw-w64-i686-dev
apt install mingw-w64-tools
apt install g++-mingw-w64-i686
cmake -DCMAKE_TOOLCHAIN_FILE=mingw-w64-i686.cmake . & make