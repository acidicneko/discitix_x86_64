#include "drivers/tty/tty.h"
#include "drivers/tty/font.h"
#include "drivers/tty/hansi_parser.h"
#include "drivers/framebuffer.h"

uint32_t colors[16];

uint32_t currentBg;
uint32_t currentFg;

uint32_t x_cursor;
uint32_t y_cursor;

void init_tty(struct stivale2_struct* bootinfo){
    init_framebuffer(bootinfo);
    x_cursor = 0;
    y_cursor = 0;
}

void init_colors(uint32_t black, uint32_t red, uint32_t green, uint32_t yellow, uint32_t blue, uint32_t purple, uint32_t cyan, uint32_t white,
                         uint32_t blackBright, uint32_t redBright, uint32_t greenBright, uint32_t yellowBright, uint32_t blueBright, uint32_t purpleBright, uint32_t cyanBright, uint32_t whiteBright){
    // normal colors
    colors[0] = black;
    colors[1] = red;
    colors[2] = green;
    colors[3] = yellow;
    colors[4] = blue;
    colors[5] = purple;
    colors[6] = cyan;
    colors[7] = white;

    // bright colors
    colors[8] = blackBright;
    colors[9] = redBright;
    colors[10] = greenBright;
    colors[11] = yellowBright;
    colors[12] = blueBright;
    colors[13] = purpleBright;
    colors[14] = cyanBright;
    colors[15] = whiteBright;

    currentBg = colors[0];
    currentFg = colors[7];

    framebuffer_clear(currentBg);
}

void tty_paint_cell(terminal_cell_t cell){
    uint8_t iy, ix;
        // paint the background of cell
        for(iy = 0; iy < 8; iy++){
            for(ix = 0; ix < 8; ix++){
                if((font[128][iy] >> ix) & 1){
                    framebuffer_put_pixel(ix + x_cursor*GLYPH_WIDTH, iy + y_cursor*GLYPH_HEIGHT, cell.bg);
                }
            }
        }
        // paint the printable character
        for(iy = 0; iy < 8; iy++){
            for(ix = 0; ix < 8; ix++){
                if((font[(uint8_t)cell.printable_char][iy] >> ix) & 1){
                    framebuffer_put_pixel(ix + x_cursor*GLYPH_WIDTH, iy + y_cursor*GLYPH_HEIGHT, cell.fg);
                }
            }
        }
}

void tty_putchar_raw(char c){
    if(c == '\n'){
        x_cursor = 0;
        y_cursor++;
    }
    else if(c == '\r')
        x_cursor = 0;
    else if(c == '\t')
        x_cursor = (x_cursor - (x_cursor % 8)) + 8;

    else if(c == '\b'){
        x_cursor--;
        tty_putchar_raw(' ');
        x_cursor--;
    }
    else{
        terminal_cell_t cell = {
            .printable_char = c,
            .fg = currentFg,
            .bg = currentBg
        };
        tty_paint_cell(cell);
        x_cursor += 1;
    }
    if(x_cursor >= framebuffer_width/GLYPH_WIDTH){
        x_cursor = 0;
        y_cursor++;
    }
}

void tty_putchar(char c){
    hansi_handler(c);
}

void set_currentFg(uint32_t value){
    currentFg = value;
}

void set_currentBg(uint32_t value){
    currentBg = value;
}
