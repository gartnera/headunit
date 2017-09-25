#!/bin/bash
#update the temp paths as required
OUTDIR=/home/lucas/libunwindbuild
M3TOOLCHAIN=/home/lucas/m3-toolchain-lmagder

./configure --prefix=$OUTDIR --enable-static --disable-shared --with-sysroot=$M3TOOLCHAIN/arm-cortexa9_neon-linux-gnueabi/sysroot --target=arm-cortexa9_neon-linux-gnueabi --host=arm-cortexa9_neon-linux-gnueabi --enable-cxx-exceptions PATH=$PATH:$M3TOOLCHAIN/bin
make install PATH=$PATH:$M3TOOLCHAIN/bin
