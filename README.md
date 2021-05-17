# Discitix Kernel

A hobbyist kernel written in C!

## About
Discitix is a kernel being written to wander in the world of low level!
Discitix aims to be POSIX-compilant in future.
Pardon if you find any nonsense in my code :p
Join Discitix's own [Discord Server](https://discord.gg/6a9C3r2fGU)

## Discitix in action

![Settings Window](https://raw.githubusercontent.com/ayush7788/discitix_x86_64/main/images/prompt.png)
![Settings Window](https://raw.githubusercontent.com/ayush7788/discitix_x86_64/main/images/serial.png)

## Building
Please install libuuid, libfuse and pkgconfig for echFS utils.
Install these too:
- QEMU
- GNU Make
- GCC 10+
- NASM
- Parted

After installing all prerequsites, clone this repo with -
```
git clone --recurse-submodules https://github.com/ayush7788/discitix_x86_64.git
```
If you are building for first time, run `make setup` as root.

To build the kernel do `make` in project's root.

After building the kernel, run it with, `make run`

### Features
- [x] Port to x86_64
- [x] High resolution framebuffer driver
- [x] Physical Memory Manager
- [x] Interrupts implemented
- [ ] Virtual Memory Manager
- [ ] Heap memory
- [ ] VFS
- [ ] HDD driver
- [ ] echFS
- [ ] Syscalls
- [ ] ELF execution
- [ ] Process Management