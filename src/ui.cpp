#include "ui.h"
#include "globals.h"
#include "lcd.h"
#include <stdio.h>

void render_main_menu() {
    lcd_clear(0x122A);
    lcd_draw_string(140, 20, "PLOTTER MENU", 0xFFFF, 0x122A, 3);
    lcd_draw_rect(40, 80, 440, 140, 0x39E7);   lcd_draw_string(60, 100, "1. SD CARD PRINT MODE", 0x0000, 0x39E7, 2);
    lcd_draw_rect(40, 160, 440, 220, 0x39E7);  lcd_draw_string(60, 180, "2. SET HOME POSITION", 0x0000, 0x39E7, 2);
    lcd_draw_rect(40, 240, 440, 300, 0x39E7);  lcd_draw_string(60, 260, "3. GO HOME", 0x0000, 0x39E7, 2);
}

void render_sd_menu() {
    lcd_clear(0x0000); 

    if (totalFiles == 0) {
        if (sd_err != 0) { // FR_OK is 0
            char errMsg[30];
            sprintf(errMsg, "ERR: %s %d", err_stage, sd_err);
            lcd_draw_string(100, 140, errMsg, 0xF800, 0x0000, 3);
        } else {
            lcd_draw_string(160, 140, "EMPTY SD", 0xF800, 0x0000, 3);
        }
        return;
    }

    lcd_draw_string(110, 20, "SELECT FILE", 0xFFFF, 0x0000, 2);
    for (uint8_t i = 0; i < totalFiles; i++) {
        uint16_t box_color = (i == currentSelection) ? 0x07E0 : 0x31A6; 
        uint16_t text_color = (i == currentSelection) ? 0x0000 : 0xFFFF;
        
        lcd_draw_rect(40, 70 + (i * 50), 440, 110 + (i * 50), box_color);
        lcd_draw_string(60, 85 + (i * 50), fileList[i], text_color, box_color, 2);
    }
}

void render_touch_draw() {
    lcd_clear(0x0000);
    lcd_draw_string(100, 140, "TOUCH DRAW ACTIVE", 0x07E0, 0x0000, 3);
}

void render_home() {
    lcd_clear(0x0000);
    lcd_draw_string(100, 140, "SETTING HOME...", 0xFFE0, 0x0000, 3);
}
