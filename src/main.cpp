#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdio.h>

// Includem libraria Petit FatFs
#include "pff.h"

// Prototipuri obligatorii
void init_lcd_hardware(void);

// ==========================================
// CONFIGURARE PINI CONTROL LCD (PORTF)
// ==========================================
#define LCD_CTRL_DDR   DDRF
#define LCD_CTRL_PORT  PORTF
#define LCD_RD         PF0
#define LCD_WR         PF1
#define LCD_RS         PF2
#define LCD_CS         PF3
#define LCD_RST        PF4


#define MAX_FISIERE 10
#define LUNGIME_NUME 13
char listaFisiere[MAX_FISIERE][LUNGIME_NUME];
uint8_t numarFisiereTotal = 0;

// Fontul 8x8 stocat în PROGMEM
const uint8_t font8x8[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [32] Spațiu
    0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x10, 0x00, // [33] !
    0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [34] "
    0x24, 0x24, 0x7F, 0x24, 0x7F, 0x24, 0x24, 0x00, // [35] #
    0x12, 0x3C, 0x50, 0x38, 0x05, 0x3C, 0x48, 0x00, // [36] $
    0x62, 0x66, 0x0C, 0x18, 0x30, 0x66, 0x46, 0x00, // [37] %
    0x38, 0x44, 0x38, 0x6C, 0x44, 0x44, 0x3A, 0x00, // [38] &
    0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [39] '
    0x08, 0x10, 0x20, 0x20, 0x20, 0x10, 0x08, 0x00, // [40] (
    0x10, 0x08, 0x04, 0x04, 0x04, 0x08, 0x10, 0x00, // [41] )
    0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, 0x00, 0x00, // [42] *
    0x00, 0x10, 0x10, 0x7C, 0x10, 0x10, 0x00, 0x00, // [43] +
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x20, // [44] ,
    0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, // [45] -
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, // [46] .
    0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00, // [47] /
    0x38, 0x44, 0x4C, 0x54, 0x64, 0x44, 0x38, 0x00, // [48] 0
    0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, // [49] 1
    0x38, 0x44, 0x04, 0x08, 0x10, 0x20, 0x7C, 0x00, // [50] 2
    0x38, 0x44, 0x04, 0x18, 0x04, 0x44, 0x38, 0x00, // [51] 3
    0x08, 0x18, 0x28, 0x48, 0x7C, 0x08, 0x08, 0x00, // [52] 4
    0x7C, 0x40, 0x78, 0x04, 0x04, 0x44, 0x38, 0x00, // [53] 5
    0x38, 0x40, 0x78, 0x44, 0x44, 0x44, 0x38, 0x00, // [54] 6
    0x7C, 0x04, 0x08, 0x10, 0x20, 0x20, 0x20, 0x00, // [55] 7
    0x38, 0x44, 0x44, 0x38, 0x44, 0x44, 0x38, 0x00, // [56] 8
    0x38, 0x44, 0x44, 0x3C, 0x04, 0x04, 0x38, 0x00, // [57] 9
    0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, // [58] :
    0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00, // [59] ;
    0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00, // [60] <
    0x00, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x00, 0x00, // [61] =
    0x20, 0x10, 0x08, 0x04, 0x08, 0x10, 0x20, 0x00, // [62] >
    0x38, 0x44, 0x04, 0x08, 0x10, 0x00, 0x10, 0x00, // [63] ?
    0x38, 0x44, 0x54, 0x54, 0x5C, 0x40, 0x3C, 0x00, // [64] @
    0x10, 0x28, 0x44, 0x44, 0x7C, 0x44, 0x44, 0x00, // [65] A
    0x78, 0x44, 0x44, 0x78, 0x44, 0x44, 0x78, 0x00, // [66] B
    0x38, 0x44, 0x40, 0x40, 0x40, 0x44, 0x38, 0x00, // [67] C
    0x70, 0x48, 0x44, 0x44, 0x44, 0x48, 0x70, 0x00, // [68] D
    0x7C, 0x40, 0x40, 0x78, 0x40, 0x40, 0x7C, 0x00, // [69] E
    0x7C, 0x40, 0x40, 0x78, 0x40, 0x40, 0x40, 0x00, // [70] F
    0x38, 0x44, 0x40, 0x5C, 0x44, 0x44, 0x38, 0x00, // [71] G
    0x44, 0x44, 0x44, 0x7C, 0x44, 0x44, 0x44, 0x00, // [72] H
    0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, // [73] I
    0x0C, 0x04, 0x04, 0x04, 0x04, 0x44, 0x38, 0x00, // [74] J
    0x44, 0x48, 0x50, 0x60, 0x50, 0x48, 0x44, 0x00, // [75] K
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7C, 0x00, // [76] L
    0x44, 0x6C, 0x54, 0x54, 0x44, 0x44, 0x44, 0x00, // [77] M
    0x44, 0x64, 0x54, 0x4C, 0x44, 0x44, 0x44, 0x00, // [78] N
    0x38, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38, 0x00, // [79] O
    0x78, 0x44, 0x44, 0x78, 0x40, 0x40, 0x40, 0x00, // [80] P
    0x38, 0x44, 0x44, 0x44, 0x54, 0x48, 0x34, 0x00, // [81] Q
    0x78, 0x44, 0x44, 0x78, 0x50, 0x48, 0x44, 0x00, // [82] R
    0x38, 0x44, 0x40, 0x38, 0x04, 0x44, 0x38, 0x00, // [83] S
    0x7C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, // [84] T
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38, 0x00, // [85] U
    0x44, 0x44, 0x44, 0x44, 0x28, 0x28, 0x10, 0x00, // [86] V
    0x44, 0x44, 0x44, 0x54, 0x54, 0x6C, 0x44, 0x00, // [87] W
    0x44, 0x44, 0x28, 0x10, 0x28, 0x44, 0x44, 0x00, // [88] X
    0x44, 0x44, 0x28, 0x10, 0x10, 0x10, 0x10, 0x00, // [89] Y
    0x7C, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7C, 0x00  // [90] Z
};

// ==========================================
// DRIVER LOW-LEVEL LCD 
// ==========================================
void init_lcd_hardware(void) {
    LCD_CTRL_DDR |= (1 << LCD_RD) | (1 << LCD_WR) | (1 << LCD_RS) | (1 << LCD_CS) | (1 << LCD_RST);
    LCD_CTRL_PORT |= (1 << LCD_RD) | (1 << LCD_WR) | (1 << LCD_RS) | (1 << LCD_CS) | (1 << LCD_RST);
    DDRE |= (1 << PE3) | (1 << PE4) | (1 << PE5);
    DDRG |= (1 << PG5);
    DDRH |= (1 << PH3) | (1 << PH4) | (1 << PH5) | (1 << PH6);
}

void lcd_write_bus(uint8_t data) {
    PORTE &= ~((1 << PE4) | (1 << PE5) | (1 << PE3)); PORTG &= ~(1 << PG5); PORTH &= ~((1 << PH3) | (1 << PH4) | (1 << PH5) | (1 << PH6));
    if (data & (1 << 0)) PORTH |= (1 << PH5); if (data & (1 << 1)) PORTH |= (1 << PH6); if (data & (1 << 2)) PORTE |= (1 << PE4); if (data & (1 << 3)) PORTE |= (1 << PE5);
    if (data & (1 << 4)) PORTG |= (1 << PG5); if (data & (1 << 5)) PORTE |= (1 << PE3); if (data & (1 << 6)) PORTH |= (1 << PH3); if (data & (1 << 7)) PORTH |= (1 << PH4);
    LCD_CTRL_PORT &= ~(1 << LCD_WR); asm volatile("nop"); LCD_CTRL_PORT |= (1 << LCD_WR);
}

void lcd_write_command(uint8_t cmd) { LCD_CTRL_PORT &= ~(1 << LCD_RS); LCD_CTRL_PORT &= ~(1 << LCD_CS); lcd_write_bus(cmd); LCD_CTRL_PORT |= (1 << LCD_CS); }
void lcd_write_data(uint8_t data) { LCD_CTRL_PORT |= (1 << LCD_RS); LCD_CTRL_PORT &= ~(1 << LCD_CS); lcd_write_bus(data); LCD_CTRL_PORT |= (1 << LCD_CS); }

void lcd_init(void) {
    init_lcd_hardware();
    LCD_CTRL_PORT |= (1 << LCD_RST); _delay_ms(10); LCD_CTRL_PORT &= ~(1 << LCD_RST); _delay_ms(50); LCD_CTRL_PORT |= (1 << LCD_RST); _delay_ms(120);
    lcd_write_command(0x11); _delay_ms(150); lcd_write_command(0x3A); lcd_write_data(0x55); lcd_write_command(0x36); lcd_write_data(0x28); lcd_write_command(0x29); _delay_ms(50);
}

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    lcd_write_command(0x2A); lcd_write_data(x0 >> 8); lcd_write_data(x0 & 0xFF); lcd_write_data(x1 >> 8); lcd_write_data(x1 & 0xFF);
    lcd_write_command(0x2B); lcd_write_data(y0 >> 8); lcd_write_data(y0 & 0xFF); lcd_write_data(y1 >> 8); lcd_write_data(y1 & 0xFF);
    lcd_write_command(0x2C);
}

void lcd_clear(uint16_t color) {
    lcd_set_window(0, 0, 479, 319); LCD_CTRL_PORT |= (1 << LCD_RS); LCD_CTRL_PORT &= ~(1 << LCD_CS);
    uint8_t hi = color >> 8; uint8_t lo = color & 0xFF;
    for (uint32_t i = 0; i < 153600; i++) { lcd_write_bus(hi); lcd_write_bus(lo); }
    LCD_CTRL_PORT |= (1 << LCD_CS);
}

void lcd_draw_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    lcd_set_window(x0, y0, x1, y1); LCD_CTRL_PORT |= (1 << LCD_RS); LCD_CTRL_PORT &= ~(1 << LCD_CS);
    uint8_t hi = color >> 8; uint8_t lo = color & 0xFF; uint32_t total = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);
    for (uint32_t i = 0; i < total; i++) { lcd_write_bus(hi); lcd_write_bus(lo); }
    LCD_CTRL_PORT |= (1 << LCD_CS);
}

void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg_color, uint8_t size) {
    if (c < 32 || c > 90) {
        c = 32;
    }
    uint16_t offset = (c - 32) * 8; 
    uint8_t line;
    lcd_set_window(x, y, x + (8 * size) - 1, y + (8 * size) - 1); LCD_CTRL_PORT |= (1 << LCD_RS); LCD_CTRL_PORT &= ~(1 << LCD_CS);
    for (uint8_t r = 0; r < 8; r++) {
        line = pgm_read_byte(&(font8x8[offset + r]));
        for (uint8_t i = 0; i < size; i++) {
            for (uint8_t b = 0; b < 8; b++) {
                uint16_t p_color = (line & (1 << (7 - b))) ? color : bg_color;
                uint8_t hi = p_color >> 8; uint8_t lo = p_color & 0xFF;
                for (uint8_t j = 0; j < size; j++) { lcd_write_bus(hi); lcd_write_bus(lo); }
            }
        }
    }
    LCD_CTRL_PORT |= (1 << LCD_CS);
}

void lcd_draw_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color, uint8_t size) {
    while (*str) { lcd_draw_char(x, y, *str, color, bg_color, size); x += 8 * size; str++; }
}

// ==========================================
// CITIRE DIRECTOARE PRIN LIBRĂRIA PETIT FATFS
// ==========================================
FATFS fs;
int sd_err = 0;
char err_stage[10] = "";

void incarca_lista_sd_petit(void) {
    numarFisiereTotal = 0;

    sd_err = pf_mount(&fs);
    if (sd_err != FR_OK) {
        strcpy(err_stage, "MOUNT");
        return; 
    }

    DIR dir;
    sd_err = pf_opendir(&dir, "/");
    if (sd_err != FR_OK) {
        sd_err = pf_opendir(&dir, ""); // Fallback
        if (sd_err != FR_OK) {
            strcpy(err_stage, "OPENDIR");
            return;
        }
    }

    FILINFO fno;
    while (numarFisiereTotal < MAX_FISIERE) {
        sd_err = pf_readdir(&dir, &fno);
        if (sd_err != FR_OK) {
            strcpy(err_stage, "READDIR");
            break;
        }
        if (fno.fname[0] == 0) {
            break;
        }

        if (!(fno.fattrib & AM_DIR)) {
            char* numeComplet = fno.fname;
            if (strstr(numeComplet, ".GCO") != NULL || 
                strstr(numeComplet, ".gco") != NULL || 
                strstr(numeComplet, ".GCODE") != NULL || 
                strstr(numeComplet, ".gcode") != NULL) {
                
                strncpy(listaFisiere[numarFisiereTotal], numeComplet, LUNGIME_NUME - 1);
                listaFisiere[numarFisiereTotal][LUNGIME_NUME - 1] = '\0';
                numarFisiereTotal++;
            }
            else if (numarFisiereTotal == 0 && strlen(numeComplet) > 0) {
                strncpy(listaFisiere[numarFisiereTotal], numeComplet, LUNGIME_NUME - 1);
                listaFisiere[numarFisiereTotal][LUNGIME_NUME - 1] = '\0';
                numarFisiereTotal = 1;
                break;
            }
        }
    }
}

void randeaza_interfata_sd(int selectie) {
    lcd_clear(0x0000); 

    if (numarFisiereTotal == 0) {
        if (sd_err != FR_OK) {
            char errMsg[30];
            sprintf(errMsg, "ERR: %s %d", err_stage, sd_err);
            lcd_draw_string(100, 140, errMsg, 0xF800, 0x0000, 3);
        } else {
            lcd_draw_string(160, 140, "EMPTY SD", 0xF800, 0x0000, 3);
        }
        return;
    }

    lcd_draw_string(110, 20, "SELECTEAZA FISIER", 0xFFFF, 0x0000, 2);
    for (uint8_t i = 0; i < numarFisiereTotal; i++) {
        uint16_t box_color = (i == selectie) ? 0x07E0 : 0x31A6; 
        uint16_t text_color = (i == selectie) ? 0x0000 : 0xFFFF;
        
        lcd_draw_rect(40, 70 + (i * 50), 440, 110 + (i * 50), box_color);
        lcd_draw_string(60, 85 + (i * 50), listaFisiere[i], text_color, box_color, 2);
    }
}

int main(void) {
    lcd_init();
    
    lcd_clear(0x122A);
    lcd_draw_string(140, 20, "MENIU PLOTTER", 0xFFFF, 0x122A, 3);
    lcd_draw_rect(40, 80, 440, 140, 0x39E7);   lcd_draw_string(60, 100, "1. MOD TOUCH DRAW", 0x0000, 0x39E7, 2);
    lcd_draw_rect(40, 160, 440, 220, 0x39E7);  lcd_draw_string(60, 180, "2. MOD PRINT SD CARD", 0x0000, 0x39E7, 2);
    lcd_draw_rect(40, 240, 440, 300, 0x39E7);  lcd_draw_string(60, 260, "3. SET POZITIE HOME", 0x0000, 0x39E7, 2);

    _delay_ms(3000); 
    
    int selectieCurenta = 0;
    incarca_lista_sd_petit(); 

    while (1) {
        randeaza_interfata_sd(selectieCurenta);
        while(1) {
            _delay_ms(100);
        }
    }
}