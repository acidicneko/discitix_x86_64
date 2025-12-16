# Discitix Kernel

A hobbyist kernel written in C!

## About
Discitix is a kernel being written to wander in the world of low level!
Discitix aims to be POSIX-compilant in future.
Pardon if you find any nonsense in my code :p
Join Discitix's own [Discord Server](https://discord.gg/6a9C3r2fGU)

## Discitix in action

![Settings Window](https://raw.githubusercontent.com/acidicneko/discitix_x86_64/main/images/kernel.png)

## Building

Clone this repo with -
```
git clone --recurse-submodules https://github.com/acidicneko/discitix_x86_64.git
```

### Using Nix shell configuration
You can use the Nix shell configuration to build the kernel.

Enter the nix shell with the following command:
```bash
nix-shell
```

Run the following command the first time to set up the environment:
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

### Natively on system
Please install libuuid, libfuse and pkgconfig for echFS utils.
Install these too:
- QEMU
- GNU Make
- GCC 10+
- NASM
- Parted

After installing all prerequsites, if you are building for first time, run `make setup` as root.

To build the kernel do `make` in project's root.

After building the kernel, run it with, `make run`

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
- [ ] echFS
- [ ] Syscalls
- [ ] ELF execution
- [x] Process Management
