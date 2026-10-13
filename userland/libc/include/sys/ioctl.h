// usr/include/sys/ioctl.h — minimal stub
#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H
#define TIOCGWINSZ 0x5413
struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; };
int ioctl(int fd, unsigned long request, ...);
#endif
