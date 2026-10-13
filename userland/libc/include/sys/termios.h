// usr/include/sys/termios.h
#ifndef _SYS_TERMIOS_H
#define _SYS_TERMIOS_H

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t     c_cc[NCCS];
    speed_t  c_ispeed;
    speed_t  c_ospeed;
};

/* c_cc indices */
#define VINTR    0
#define VQUIT    1
#define VERASE   2
#define VKILL    3
#define VEOF     4
#define VTIME    5
#define VMIN     6
#define VSTART   7
#define VSTOP    8
#define VSUSP    9
#define VEOL     10
#define VREPRINT 11
#define VDISCARD 12
#define VWERASE  13
#define VLNEXT   14
#define VEOL2    15

/* c_iflag bits */
#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IXON    0002000
#define IXOFF   0010000

/* c_oflag bits */
#define OPOST   0000001
#define ONLCR   0000004

/* c_cflag bits */
#define CS8     0000060
#define CREAD   0000200
#define CLOCAL  0004000

/* c_lflag bits */
#define ISIG    0000001
#define ICANON  0000002
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define NOFLSH  0000200
#define TOSTOP  0000400
#define IEXTEN  0100000

/* tcsetattr actions */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* tcflush queue selectors */
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int action, const struct termios *t);
int tcflush(int fd, int queue);
speed_t cfgetispeed(const struct termios *t);
speed_t cfgetospeed(const struct termios *t);
int cfsetispeed(struct termios *t, speed_t speed);
int cfsetospeed(struct termios *t, speed_t speed);

#endif
