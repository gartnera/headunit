#update the temp paths as required
OUTDIR=/home/lucas/protobuild
M3TOOLCHAIN=/home/lucas/m3-toolchain-lmagder

./configure --prefix=$OUTDIR --enable-static --disable-shared CXXFLAGS=-static-libstdc++ LDFLAGS=-static-libstdc++
make install CXXFLAGS=-static-libstdc++ LDFLAGS=-static-libstdc++
make clean
rm -r $OUTDIR/lib/*
rm -r $OUTDIR/include/*
#copy arm version on top, old protoc stays since it doesn't have the target prefix
./configure --prefix=$OUTDIR --enable-static --disable-shared --with-sysroot=$M3TOOLCHAIN/arm-cortexa9_neon-linux-gnueabi/sysroot --target=arm-cortexa9_neon-linux-gnueabi --host=arm-cortexa9_neon-linux-gnueabi --enable-cross-compile --with-protoc=$OUTDIR/bin/protoc PATH=$PATH:$M3TOOLCHAIN/bin CXXFLAGS=-static-libstdc++ LDFLAGS=-static-libstdc++
make install PATH=$PATH:$M3TOOLCHAIN/bin CXXFLAGS=-static-libstdc++ LDFLAGS=-static-libstdc++
