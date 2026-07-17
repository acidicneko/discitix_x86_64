#!/bin/bash

if ! command -v x86_64-linux-gnu-gcc &> /dev/null; then
  echo "Cross compiler toolchain not found!"
  exit 1
fi

echo "Downloading newlib source..."

NEWLIB_SRC="https://sourceware.org/pub/newlib/newlib-4.6.0.20260123.tar.gz"
wget $(NEWLIB_SRC)
tar -xf newlib-4.6.0.20260123.tar.gz

mkdir newlib_discitix
cd newlib_discitix

echo "Compiling newlib source..."

CC="gcc" \
CC_FOR_TARGET="x86_64-linux-gnu-gcc -ffreestanding" \
AS_FOR_TARGET="x86_64-linux-gnu-as" \
LD_FOR_TARGET="x86_64-linux-gnu-ld" \
AR_FOR_TARGET="x86_64-linux-gnu-ar" \
RANLIB_FOR_TARGET="x86_64-linux-gnu-ranlib" \
../newlib-4.6.0.20260123/configure \
    --target=x86_64-elf \
    --prefix=$PWD/newlib_discitix \
    --disable-newlib-supplied-syscalls \
    --disable-nls \
    --enable-newlib-io-long-long \
    --enable-newlib-register-fini \
    --disable-multilib


make -j$(nproc)
make install

cd ..

mkdir -p source/usr/
cp -r newlib_discitix/x86_64-elf/include source/usr/
cp -r newlib_discitix/x86_64-elf/lib source/usr/

cd source
echo "Compiling Discitix runtime..."

x86_64-linux-gnu-gcc -Iusr/include -nostdlib -ffreestanding -mno-red-zone -fno-pic -no-pie -c crt.c -o usr/lib/crt0.o
x86_64-linux-gnu-gcc -Iusr/include -nostdlib -ffreestanding -mno-red-zone -fno-pic -no-pie -c stubs.c -o usr/lib/stubs.o
x86_64-linux-gnu-ar rcs usr/lib/libc.a usr/lib/stubs.o

echo "Done!"
