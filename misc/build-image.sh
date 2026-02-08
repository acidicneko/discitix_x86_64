#!/usr/bin/env bash
set -euo pipefail

IMAGE="build/image.hdd"
KERNEL="build/kernel.elf"

echo "[CREATE IMAGE]"

dd if=/dev/zero of="$IMAGE" bs=1M count=64

parted -s "$IMAGE" mklabel msdos
parted -s "$IMAGE" mkpart primary fat32 1MiB 100%

LOOPDEV=$(sudo losetup --find --show --partscan "$IMAGE")

sudo mkfs.fat -F32 "${LOOPDEV}p1"

mkdir -p build/mnt
sudo mount "${LOOPDEV}p1" build/mnt

sudo cp "$KERNEL" build/mnt/kernel.elf
sudo cp misc/initrd/initrd.img build/mnt/initrd.img
sudo cp misc/limine.conf build/mnt/limine.conf
sudo cp limine/limine*.sys build/mnt/

sudo ./limine/limine bios-install "$IMAGE"

sudo umount build/mnt
sudo losetup -d "$LOOPDEV"

echo "[DONE]"
