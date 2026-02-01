/*
Copyright© Ayush Yadav 2021 under MIT License.
Hobby ANSI Parser(HANSIP) is forked from VTConsole, made by https://github.com/sleepy-monax
HANSIP is a modified version of VTConsole.
But instead of a terminal, it is a ANSI Parser only.
*/
#include <drivers/tty/hansi_parser.h>
#include <drivers/tty/tty.h>
#include <drivers/tty/psf2.h>
#include <drivers/framebuffer.h>

hansi_parser g_parser = {
    .state = HTERM_ESC,
    .index = 0
}; // global parser object

bool isdigit(char c){
    if(c >= '0' && c <= '9'){
        return true;
    }
    return false;
}

void hansi_handle_sgr(ansi_args* stack, int count){
    static bool fg_bold = false;
    static bool bg_bold = false;
    for (int i = 0; i < count; i++)
    {
        if (stack[i].empty || stack[i].value == 0)
        {
            set_currentFg(colors[7]);
            set_currentBg(colors[0]);
            fg_bold = false;
            bg_bold = false;
        }
        else
        {
            int attr = stack[i].value;
            int value;
            if (attr == 1)
            {
                fg_bold = true;
            }
            else if (attr == 21)
            {
                bg_bold = true;
            }
            else if (attr >= 30 && attr <= 37)
            {
                if(fg_bold)    value = (attr - 30) + 8;
                else value = attr - 30;
                set_currentFg(colors[value]);
            }
            else if (attr >= 40 && attr <= 47)
            {
                if(bg_bold)    value = (attr - 40) + 8;
                else value = attr - 40;
                set_currentBg(colors[value]);
            }
        }
    }
}

void hansi_handler(char c){
    if(g_parser.state == HTERM_ESC){
        if(c == '\033'){
            g_parser.state = HTERM_BRACKET;
            g_parser.index = 0;
            g_parser.args[g_parser.index].value = 0;
            g_parser.args[g_parser.index].empty = true;
        }
        else{
            g_parser.state = HTERM_ESC;
            tty_putchar_raw(c);
        }
    }
    else if(g_parser.state == HTERM_BRACKET){
        if(c == '['){
            g_parser.state = HTERM_ARGS;
        }
        else{
            g_parser.state = HTERM_ESC;
            tty_putchar_raw(c);
        }
    }
    else if(g_parser.state == HTERM_ARGS){
        if(isdigit(c)){
            g_parser.args[g_parser.index].value *= 10;
            g_parser.args[g_parser.index].value += (c - '0');
            g_parser.args[g_parser.index].empty = false;
        }
        else{
            if(g_parser.index < MAX_ARGS)
                g_parser.index++;
            g_parser.args[g_parser.index].value = 0;
            g_parser.args[g_parser.index].empty = true;
            g_parser.state = HTERM_ENDARGS;
        }
    }
    if(g_parser.state == HTERM_ENDARGS){
        if(c == ';'){
            g_parser.state = HTERM_ARGS;
        }
        else{
            int n, m;
            tty_t* tty = get_current_tty();
            switch(c){
                case 'm': // SGR - Select Graphic Rendition
                    hansi_handle_sgr(&g_parser.args[0], g_parser.index);
                    break;
                case 'A': // CUU - Cursor Up
                    if (!tty) break;
                    n = g_parser.args[0].empty ? 1 : g_parser.args[0].value;
                    if (n < 1) n = 1;
                    if (tty->y_cursor >= (uint16_t)n) {
                        tty->y_cursor -= n;
                    } else {
                        tty->y_cursor = 0;
                    }
                    break;
                case 'B': // CUD - Cursor Down
                    if (!tty) break;
                    n = g_parser.args[0].empty ? 1 : g_parser.args[0].value;
                    if (n < 1) n = 1;
                    tty->y_cursor += n;
                    if (tty->y_cursor >= tty->height) {
                        tty->y_cursor = tty->height - 1;
                    }
                    break;
                case 'C': // CUF - Cursor Forward
                    if (!tty) break;
                    n = g_parser.args[0].empty ? 1 : g_parser.args[0].value;
                    if (n < 1) n = 1;
                    tty->x_cursor += n;
                    if (tty->x_cursor >= tty->width) {
                        tty->x_cursor = tty->width - 1;
                    }
                    break;
                case 'D': // CUB - Cursor Back
                    if (!tty) break;
                    n = g_parser.args[0].empty ? 1 : g_parser.args[0].value;
                    if (n < 1) n = 1;
                    if (tty->x_cursor >= (uint16_t)n) {
                        tty->x_cursor -= n;
                    } else {
                        tty->x_cursor = 0;
                    }
                    break;
                case 'H': // CUP - Cursor Position
                case 'f': // HVP - same as CUP
                    if (!tty) break;
                    n = g_parser.args[0].empty ? 1 : g_parser.args[0].value; // row (1-based)
                    m = g_parser.args[1].empty ? 1 : g_parser.args[1].value; // col (1-based)
                    if (n < 1) n = 1;
                    if (m < 1) m = 1;
                    tty->y_cursor = n - 1; // Convert to 0-based
                    tty->x_cursor = m - 1;
                    // Clamp to screen bounds
                    if (tty->y_cursor >= tty->height) {
                        tty->y_cursor = tty->height - 1;
                    }
                    if (tty->x_cursor >= tty->width) {
                        tty->x_cursor = tty->width - 1;
                    }
                    break;
                case 'J': // ED - Erase in Display
                    n = g_parser.args[0].empty ? 0 : g_parser.args[0].value;
                    if (n == 2) {
                        // Clear entire screen
                        tty_clear();
                    }
                    // TODO: n=0 clear from cursor to end, n=1 clear from start to cursor
                    break;
                case 'K': // EL - Erase in Line
                    // TODO: implement line erase
                    break;
                default:
                    break;
            }
            g_parser.state = HTERM_ESC;
        }
    }
}