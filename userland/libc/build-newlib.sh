#!/usr/bin/env bash
# Download and build newlib for Discitix.
#
#   ./build-newlib.sh [PREFIX]
#
# PREFIX defaults to userland/newlib. The libc Makefile copies headers and
# libraries out of PREFIX/x86_64-elf into the build sysroot.
set -euo pipefail

VERSION=${NEWLIB_VERSION:-4.6.0.20260123}
HERE=$(cd "$(dirname "$0")" && pwd)
PREFIX=$(realpath -m "${1:-$HERE/../newlib}")
WORK=$(realpath -m "$HERE/../newlib-src")
TARBALL=newlib-$VERSION.tar.gz

if ! command -v x86_64-linux-gnu-gcc >/dev/null; then
    echo "Cross compiler toolchain (x86_64-linux-gnu-gcc) not found!" >&2
    exit 1
fi

mkdir -p "$WORK"
cd "$WORK"

if [ ! -d "newlib-$VERSION" ]; then
    if [ ! -f "$TARBALL" ]; then
        echo "==> Downloading newlib $VERSION..."
        wget "https://sourceware.org/pub/newlib/$TARBALL"
    fi
    tar -xf "$TARBALL"
fi

echo "==> Configuring newlib (prefix: $PREFIX)..."
mkdir -p build
cd build
CC="gcc" \
CC_FOR_TARGET="x86_64-linux-gnu-gcc -ffreestanding" \
AS_FOR_TARGET="x86_64-linux-gnu-as" \
LD_FOR_TARGET="x86_64-linux-gnu-ld" \
AR_FOR_TARGET="x86_64-linux-gnu-ar" \
RANLIB_FOR_TARGET="x86_64-linux-gnu-ranlib" \
"../newlib-$VERSION/configure" \
    --target=x86_64-elf \
    --prefix="$PREFIX" \
    --disable-newlib-supplied-syscalls \
    --disable-nls \
    --enable-newlib-io-long-long \
    --enable-newlib-io-c99-formats \
    --enable-newlib-register-fini \
    --disable-multilib

echo "==> Building newlib..."
make -j"$(nproc)"
make install
echo "==> newlib installed to $PREFIX"
