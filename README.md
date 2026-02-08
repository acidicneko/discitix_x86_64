# Discitix Kernel

A hobbyist kernel written in C!

## About
Discitix is a kernel being written to wander in the world of low level!
Discitix aims to be POSIX-compilant in future.
Pardon if you find any nonsense in my code :p
Join Discitix's own [Discord Server](https://discord.gg/6a9C3r2fGU)

## Discitix in action

![Settings Window](https://raw.githubusercontent.com/acidicneko/discitix_x86_64/main/images/8_2_2026.png)

## Building

Clone this repo with -
```
git clone --recurse-submodules https://github.com/acidicneko/discitix_x86_64.git
```

### Choosing your Build Environment
#### 1. Using Nix shell configuration
You can use the Nix shell configuration to build the kernel.

Enter the nix shell with the following command:
```bash
nix-shell
```
#### 2. Natively on system
Install these dependencies via your package manager.
- QEMU
- GNU Make
- GCC 10+
- NASM
- Parted

All the commands below applies to both the environments.
#### Build process
Run the following command the first time to set up the environment:

##### With Make
```bash
make setup
```

Then, you can build the kernel with:
```bash
make
```
To run the kernel, use:
```bash
make run
```

##### With Karui
Alternatively you can use [Karui](https://github.com/acidicneko/karui) build tool.

Setup the build environment
```bash
karui -r setup
```
To build the kernel:
```bash
karui 
```
Run the kernel with 
```bash
karui -r run
```

### Features
- [x] Port to x86_64
- [x] High resolution framebuffer driver
- [x] ANSI Escape sequence handling
- [x] High resolution PSF2 Fonts
- [x] Physical Memory Manager
- [x] Interrupts implemented
- [x] Virtual Memory Manager
- [ ] Heap memory
- [x] VFS
- [ ] HDD driver
- [x] Syscalls
- [x] ELF execution
- [x] Process Management
- [x] Basic userland