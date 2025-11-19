
if [ ! -d build_win ]; then
  mkdir build_win
fi

cd build_win

cmake ../ \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
  -DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
  -DSDL3_DIR=/usr/x86_64-w64-mingw32/lib/cmake/SDL3 -DWIN_BUILD=1
make
cd ..
