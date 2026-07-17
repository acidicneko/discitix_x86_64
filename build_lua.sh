#!/bin/bash

cd lua
#x86_64-linux-gnu-gcc -O2 -Wall -I ../userland/source/usr/include/ -nostdlib -ffreestanding -mno-red-zone -fno-pic -no-pie -c *.c
x86_64-linux-gnu-gcc -O2 -Wall -nostdlib -ffreestanding -mno-red-zone -fno-pic -no-pie -c ../fini.c
x86_64-linux-gnu-gcc -nostdlib -static -no-pie -Wl,--build-id=none -T ../userland/source/apps/linker.ld \
-L ../userland/source/usr/lib -o lua ../userland/source/usr/lib/crt0.o fini.o l*.o -lm -lc
cp lua ../userland/source/lua
