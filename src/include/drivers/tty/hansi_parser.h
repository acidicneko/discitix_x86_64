#ifndef __HANSIP__
#define __HANSIP__

#define MAX_ARGS	    8
#define HTERM_ESC	    1
#define HTERM_BRACKET	2
#define HTERM_ARGS	    3

#include <stdbool.h>

typedef struct {
    int value;
	bool empty;
} ansi_args;

typedef struct{
    int state;
	int index;
	bool private_mode;
	ansi_args args[MAX_ARGS];
} hansi_parser;

struct tty;
void hansi_handler(struct tty *tty, char c);

#endif
