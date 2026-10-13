/*
Copyright© Ayush Yadav 2021 under MIT License.
Hobby ANSI Parser(HANSIP) is forked from VTConsole, made by https://github.com/sleepy-monax
HANSIP is a modified version of VTConsole.
But instead of a terminal, it is a ANSI Parser only.
*/
#include <drivers/tty/hansi_parser.h>
#include <drivers/tty/tty.h>

static int arg(hansi_parser *p, int i, int def) {
    if (i >= p->index || p->args[i].empty) return def;
    return p->args[i].value;
}

static void set_fg_index(tty_t *tty, int index) {
    tty->fg_index = index;
    tty->fg = tty->colors[index + (tty->bold ? 8 : 0)];
}

static void handle_sgr(tty_t *tty, hansi_parser *p) {
    for (int i = 0; i < p->index; i++) {
        int attr = p->args[i].empty ? 0 : p->args[i].value;

        if (attr == 0) {
            tty->bold = false;
            set_fg_index(tty, 7);
            tty->bg = tty->colors[0];
        } else if (attr == 1 || attr == 22) {
            tty->bold = (attr == 1);
            if (tty->fg_index >= 0) set_fg_index(tty, tty->fg_index);
        } else if (attr >= 30 && attr <= 37) {
            set_fg_index(tty, attr - 30);
        } else if (attr == 39) {
            set_fg_index(tty, 7);
        } else if (attr >= 40 && attr <= 47) {
            tty->bg = tty->colors[attr - 40];
        } else if (attr == 49) {
            tty->bg = tty->colors[0];
        } else if (attr >= 90 && attr <= 97) {
            tty->fg_index = -1;
            tty->fg = tty->colors[(attr - 90) + 8];
        } else if (attr >= 100 && attr <= 107) {
            tty->bg = tty->colors[(attr - 100) + 8];
        }
    }
}

static void handle_csi(tty_t *tty, hansi_parser *p, char c) {
    int w = tty->width, h = tty->height;
    size_t pos = (size_t)tty->y_cursor * w + tty->x_cursor;
    int n;

    switch (c) {
    case 'm':
        handle_sgr(tty, p);
        break;
    case 'A':
        n = arg(p, 0, 1);
        tty->y_cursor = (tty->y_cursor > n) ? tty->y_cursor - n : 0;
        break;
    case 'B':
        n = arg(p, 0, 1);
        tty->y_cursor = (tty->y_cursor + n < h) ? tty->y_cursor + n : h - 1;
        break;
    case 'C':
        n = arg(p, 0, 1);
        tty->x_cursor = (tty->x_cursor + n < w) ? tty->x_cursor + n : w - 1;
        break;
    case 'D':
        n = arg(p, 0, 1);
        tty->x_cursor = (tty->x_cursor > n) ? tty->x_cursor - n : 0;
        break;
    case 'G':
        n = arg(p, 0, 1);
        tty->x_cursor = (n >= 1 && n <= w) ? n - 1 : (n < 1 ? 0 : w - 1);
        break;
    case 'H':
    case 'f': {
        int row = arg(p, 0, 1), col = arg(p, 1, 1);
        if (row < 1) row = 1;
        if (col < 1) col = 1;
        tty->y_cursor = (row <= h) ? row - 1 : h - 1;
        tty->x_cursor = (col <= w) ? col - 1 : w - 1;
        break;
    }
    case 'J':
        switch (arg(p, 0, 0)) {
        case 0: tty_erase(tty, pos, w * h); break;
        case 1: tty_erase(tty, 0, pos + 1); break;
        case 2:
        case 3: tty_erase(tty, 0, w * h); break;
        }
        break;
    case 'K': {
        size_t line = (size_t)tty->y_cursor * w;
        switch (arg(p, 0, 0)) {
        case 0: tty_erase(tty, pos, line + w); break;
        case 1: tty_erase(tty, line, pos + 1); break;
        case 2: tty_erase(tty, line, line + w); break;
        }
        break;
    }
    default:
        break;
    }
}

void hansi_handler(tty_t *tty, char c) {
    hansi_parser *p = &tty->parser;

    switch (p->state) {
    case HTERM_BRACKET:
        if (c == '[') {
            p->state = HTERM_ARGS;
            p->index = 0;
            p->private_mode = false;
            p->args[0].value = 0;
            p->args[0].empty = true;
        } else {
            p->state = HTERM_ESC;  // unsupported ESC sequence: drop it
        }
        return;

    case HTERM_ARGS:
        if (c >= '0' && c <= '9') {
            p->args[p->index].value = p->args[p->index].value * 10 + (c - '0');
            p->args[p->index].empty = false;
        } else if (c == ';') {
            if (p->index < MAX_ARGS - 1) p->index++;
            p->args[p->index].value = 0;
            p->args[p->index].empty = true;
        } else if (c >= 0x3C && c <= 0x3F) {
            p->private_mode = true;   // ESC[? ... etc.
        } else if (c >= 0x20 && c <= 0x2F) {
            // intermediate bytes: ignored
        } else if (c >= 0x40 && c <= 0x7E) {
            p->index++;
            if (!p->private_mode) handle_csi(tty, p, c);
            p->state = HTERM_ESC;
        } else {
            p->state = HTERM_ESC;     // malformed: give up on the sequence
            tty_put_raw(tty, c);
        }
        return;

    default:
        if (c == '\033') {
            p->state = HTERM_BRACKET;
        } else {
            tty_put_raw(tty, c);
        }
        return;
    }
}
