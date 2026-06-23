#!/bin/bash

IMAGE_PATH="misc/initrd/initrd.img"

FILES=(
    shell.nix shell.nix
    .gitmodules .gitmodules
    compile_flags.txt compile_flags.txt
    misc/default.psf font.psf
)

for dir in userland/sysroot/apps/*; do
    [ -d "$dir" ] || continue

    app=$(basename "$dir")
    binary="$dir/$app"

    if [ -f "$binary" ] && [ -x "$binary" ]; then
        FILES+=("$binary" "$app")
    fi
done

stripFS/build/stripctl "$IMAGE_PATH" "${FILES[@]}"
