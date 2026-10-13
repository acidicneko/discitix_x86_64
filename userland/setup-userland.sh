#!/bin/bash
# One-time userland setup: build newlib, then libc (stubs + crt objects)
# into the build sysroot. After this, `make` at the top level builds the
# apps and ports.
set -e
cd "$(dirname "$0")"

if ! command -v x86_64-linux-gnu-gcc &> /dev/null; then
  echo "Cross compiler toolchain not found!"
  exit 1
fi

./libc/build-newlib.sh
make -C libc
echo "Done!"
