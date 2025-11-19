
if [ ! -d build_depend ]; then
  mkdir build_depend
fi
cd build_depend
if [ ! -d zlib ]; then
  mkdir zlib
  cd zlib
  curl -LO https://zlib.net/zlib-1.3.1.tar.gz
  tar xf zlib-1.3.1.tar.gz
  cd zlib-1.3.1
  CC=x86_64-w64-mingw32-gcc \
  AR=x86_64-w64-mingw32-ar \
  RANLIB=x86_64-w64-mingw32-ranlib \
  ./configure --static \
  --prefix=/usr/x86_64-w64-mingw32
  make -j$(nproc)
  sudo make install
  cd ../..
fi
if [ ! -d sdl2 ]; then
  mkdir sdl2
  cd sdl2
  git clone https://github.com/libsdl-org/SDL.git
  cd SDL
  mkdir build && cd build
  cmake .. \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
    -DVIDEO_WAYLAND=OFF \
    -DVIDEO_X11=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DSDL_TEST=OFF \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON
  make -j$(nproc)
  sudo make install
  cd ../../..
fi
if [ ! -d glfw ]; then
  mkdir glfw
  cd glfw
  git clone https://github.com/glfw/glfw.git
  cd glfw
  mkdir build && cd build
  cmake .. \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
    -DBUILD_SHARED_LIBS=OFF
  make -j$(nproc)
  sudo make install
  cd ../../..
fi
if [ ! -d png ]; then
  mkdir png
  cd png
  curl -LO https://download.sourceforge.net/libpng/libpng-1.6.43.tar.gz
  tar xf libpng-1.6.43.tar.gz
  cd libpng-1.6.43
  ./configure --enable-shared=no \
    --host=x86_64-w64-mingw32 \
    --prefix=/usr/x86_64-w64-mingw32 \
    ZLIB_CFLAGS="-I/usr/x86_64-w64-mingw32/include" \
    ZLIB_LIBS="/usr/x86_64-w64-mingw32/lib/libz.a"
  make -j$(nproc)
  sudo make install
  cd ../..
fi
cd ..
