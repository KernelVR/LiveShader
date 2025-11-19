
if [ ! -d build ]; then
  mkdir build
fi

cd build

cmake ../
# cmake -DWIN_BUILD=1 ../ -DCMAKE_TOOLCHAIN_FILE=windows.cmake
make
cd ..
