#!/bin/bash

IMAGE_PATH="misc/initrd/initrd.img"

FILES=(
    # (source file) (path in initrd)
    shell.nix shell.nix 
    .gitmodules .gitmodules 
    compile_flags.txt compile_flags.txt 
    misc/default.psf font.psf 
    userland/build/hello hello 
    userland/build/cat cat 
    userland/build/sh sh 
    userland/build/ls ls 
    userland/build/dbgln dbgln
    userland/build/echo echo
    userland/build/authy authy
)

stripFS/build/stripctl $IMAGE_PATH "${FILES[@]}"
