#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Petit FatFs Library
#include "pff.h"

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

// ==========================================
// KINETIC CONFIGURATION (PORTA)
// ==========================================
#define INVERT_X_DIR false
#define INVERT_Y_DIR false
#define STEPS_PER_MM 5

#define MOTOR_PORT PORTA
#define MOTOR_DDR  DDRA
#define X_DIR_PIN  PA0
#define X_STEP_PIN PA1
#define Y_DIR_PIN  PA2
#define Y_STEP_PIN PA3

float current_x_mm = 0.0;
float current_y_mm = 0.0;
long current_step_x = 0;
long current_step_y = 0;
uint16_t feedrate_delay_us = 1000; // default delay between steps

// ==========================================
// VARIABILE SISTEM & STARI
// ==========================================
enum SystemState {
    STATE_MAIN_MENU,
    STATE_SD_MENU,
    STATE_TOUCH_DRAW,
    STATE_HOME
};

SystemState stareCurenta = STATE_MAIN_MENU;
bool necesitaRenderizare = true; // Flag for seamless screen updates
int selectieCurenta = 0;         // Highlight index in menus
bool plottingActive = false;     // True when a plot is active
bool stop_requested = false;     // True when emergency stop is requested
bool is_moving = false;          // True when a stepper is physically moving

// SD Data
#define MAX_FISIERE 10
#define LUNGIME_NUME 13
char listaFisiere[MAX_FISIERE][LUNGIME_NUME];
uint8_t numarFisiereTotal = 0;
FATFS fs;
int sd_err = 0;
char err_stage[10] = "";

// ==========================================
// PURE AVR HARDWARE UART (115200 BAUD)
// ==========================================
// Replaces Arduino's Serial.begin() for a strict AVR approach
void uart_init(uint32_t baud) {
    uint16_t ubrr = F_CPU / 8 / baud - 1;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr);
    UCSR0A = (1 << U2X0); // Double transmission speed
    UCSR0B = (1 << RXEN0) | (1 << TXEN0); // Enable Rx and Tx
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8 data bits, 1 stop bit
}

bool uart_available() {
    return (UCSR0A & (1 << RXC0));
}

char uart_read() {
    return UDR0;
}

void uart_print_char(char c) {
    while (!(UCSR0A & (1 << UDRE0))); // Wait for empty transmit buffer
    UDR0 = c;
}

void uart_print(const char* str) {
    while (*str) {
        uart_print_char(*str++);
    }
}

void print_prompt() {
    uart_print("\r\n> ");
}

void checkSerial(); // Forward declaration for Bresenham loop

// ==========================================
// MOTOR CONTROL ROUTINES
// ==========================================
void init_steppers() {
    MOTOR_DDR |= (1 << X_DIR_PIN) | (1 << X_STEP_PIN) | (1 << Y_DIR_PIN) | (1 << Y_STEP_PIN);
    MOTOR_PORT &= ~((1 << X_DIR_PIN) | (1 << X_STEP_PIN) | (1 << Y_DIR_PIN) | (1 << Y_STEP_PIN));
}

void moveTo(float target_x_mm, float target_y_mm) {
    is_moving = true;
    
    // Removed boundary constraints so the plotter can move freely
    // into negative coordinates if it wasn't powered on at the corner.

    // 2. Convert to discrete steps
    long target_step_x = (long)(target_x_mm * STEPS_PER_MM);
    long target_step_y = (long)(target_y_mm * STEPS_PER_MM);

    // 3. Bresenham initialization
    long dx = labs(target_step_x - current_step_x);
    long dy = labs(target_step_y - current_step_y);
    int sx = current_step_x < target_step_x ? 1 : -1;
    int sy = current_step_y < target_step_y ? 1 : -1;

    // Set hardware direction pins
    bool dir_x_high = (sx > 0);
    if (INVERT_X_DIR) dir_x_high = !dir_x_high;
    
    bool dir_y_high = (sy > 0);
    if (INVERT_Y_DIR) dir_y_high = !dir_y_high;

    if (dir_x_high) MOTOR_PORT |= (1 << X_DIR_PIN);
    else MOTOR_PORT &= ~(1 << X_DIR_PIN);

    if (dir_y_high) MOTOR_PORT |= (1 << Y_DIR_PIN);
    else MOTOR_PORT &= ~(1 << Y_DIR_PIN);

    long err = (dx > dy ? dx : -dy) / 2;
    long e2;

    // 4. Stepping loop
    long steps_taken = 0;
    long total_steps = (dx > dy) ? dx : dy;

    while (steps_taken < total_steps) {
        checkSerial();
        if (stop_requested) break;

        e2 = err;
        bool step_x = false;
        bool step_y = false;

        if (e2 > -dx) {
            err -= dy;
            current_step_x += sx;
            step_x = true;
        }
        if (e2 < dy) {
            err += dx;
            current_step_y += sy;
            step_y = true;
        }

        // Generate synchronous pulses
        if (step_x) MOTOR_PORT |= (1 << X_STEP_PIN);
        if (step_y) MOTOR_PORT |= (1 << Y_STEP_PIN);

        // Keep pulse HIGH for 2000us (2ms) (Matches your reference code interval=2)
        for(uint16_t d = 0; d < 2000; d += 10) {
            _delay_us(10);
            checkSerial();
            if(stop_requested) break;
        }

        if (step_x) MOTOR_PORT &= ~(1 << X_STEP_PIN);
        if (step_y) MOTOR_PORT &= ~(1 << Y_STEP_PIN);

        // Keep pulse LOW for 2000us (2ms)
        for(uint16_t d = 0; d < 2000; d += 10) {
            _delay_us(10);
            checkSerial();
            if(stop_requested) break;
        }

        steps_taken++;
    }

    // Update tracking variables exactly once at the end
    current_x_mm = (float)current_step_x / STEPS_PER_MM;
    current_y_mm = (float)current_step_y / STEPS_PER_MM;
    
    is_moving = false;
}


// ==========================================
// DATE FONT LCD (PROGMEM)
// ==========================================
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

// ==========================================
// G-CODE STREAMER & PARSER
// ==========================================
void start_plot(const char* filename) {
    if (pf_open(filename) == FR_OK) {
        plottingActive = true;
        stop_requested = false;
        uart_print("Started Plotting: ");
        uart_print(filename);
    } else {
        uart_print("Error opening file!");
    }
}

bool read_gcode_line(char* buf, int max_len) {
    int i = 0;
    UINT bw;
    char c;
    while (i < max_len - 1) {
        if (pf_read(&c, 1, &bw) != FR_OK || bw == 0) {
            if (i == 0) return false; // EOF and empty buffer
            break;
        }
        if (c == '\n' || c == '\r') {
            if (i > 0) break; // End of line reached
            continue; // Skip empty lines or carriage returns
        }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return true;
}

void process_gcode_line(const char* line) {
    // Basic parser for G0 (rapid move) and G1 (linear move)
    if ((line[0] == 'G' && line[1] == '0') || (line[0] == 'G' && line[1] == '1')) {
        float next_x = current_x_mm;
        float next_y = current_y_mm;

        const char* ptr = line + 2;
        while (*ptr) {
            if (*ptr == 'X') {
                next_x = atof(ptr + 1);
            }
            else if (*ptr == 'Y') {
                next_y = atof(ptr + 1);
            }
            ptr++;
        }
        
        moveTo(next_x, next_y);
    }
}

// ==========================================
// RENDER ROUTINES FOR EACH STATE
// ==========================================
void randeaza_meniu_principal() {
    lcd_clear(0x122A);
    lcd_draw_string(140, 20, "MENIU PLOTTER", 0xFFFF, 0x122A, 3);
    lcd_draw_rect(40, 80, 440, 140, 0x39E7);   lcd_draw_string(60, 100, "1. MOD TOUCH DRAW", 0x0000, 0x39E7, 2);
    lcd_draw_rect(40, 160, 440, 220, 0x39E7);  lcd_draw_string(60, 180, "2. MOD PRINT SD CARD", 0x0000, 0x39E7, 2);
    lcd_draw_rect(40, 240, 440, 300, 0x39E7);  lcd_draw_string(60, 260, "3. SET POZITIE HOME", 0x0000, 0x39E7, 2);
}

void randeaza_interfata_sd() {
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
        uint16_t box_color = (i == selectieCurenta) ? 0x07E0 : 0x31A6; 
        uint16_t text_color = (i == selectieCurenta) ? 0x0000 : 0xFFFF;
        
        lcd_draw_rect(40, 70 + (i * 50), 440, 110 + (i * 50), box_color);
        lcd_draw_string(60, 85 + (i * 50), listaFisiere[i], text_color, box_color, 2);
    }
}

void randeaza_touch_draw() {
    lcd_clear(0x0000);
    lcd_draw_string(100, 140, "TOUCH DRAW ACTIVE", 0x07E0, 0x0000, 3);
}

void randeaza_home() {
    lcd_clear(0x0000);
    lcd_draw_string(100, 140, "SETTING HOME...", 0xFFE0, 0x0000, 3);
}


// ==========================================
// NON-BLOCKING SERIAL PARSER (ECHO TERMINAL)
// ==========================================
#define SERIAL_BUF_SIZE 64
char serialBuffer[SERIAL_BUF_SIZE];
uint8_t serialIndex = 0;

void uart_print_int(long val) {
    char buf[12];
    ltoa(val, buf, 10);
    uart_print(buf);
}

float custom_atof(const char* str) {
    float res = 0.0;
    float frac = 1.0;
    bool in_frac = false;
    bool neg = false;
    if (*str == '-') { neg = true; str++; }
    while (*str) {
        if (*str >= '0' && *str <= '9') {
            if (in_frac) {
                frac *= 10.0;
                res += (*str - '0') / frac;
            } else {
                res = res * 10.0 + (*str - '0');
            }
        } else if (*str == '.' || *str == ',') {
            in_frac = true;
        } else if (*str == ' ' || *str == '\r' || *str == '\n') {
            // skip
        } else {
            break; // Stop parsing on other chars
        }
        str++;
    }
    return neg ? -res : res;
}

void processCommand() {
    serialBuffer[serialIndex] = '\0';
    
    // Make uppercase for case-insensitive matching
    for (uint8_t i = 0; i < serialIndex; i++) {
        if (serialBuffer[i] >= 'a' && serialBuffer[i] <= 'z') {
            serialBuffer[i] -= 32; // Convert to upper
        }
    }
    
    if (strncmp(serialBuffer, "STOP", 4) == 0) {
        stop_requested = true;
        plottingActive = false;
        uart_print("EMERGENCY STOP EXECUTED!");
    }
    else if (strncmp(serialBuffer, "BACK", 4) == 0) {
        if (stareCurenta != STATE_MAIN_MENU) {
            stareCurenta = STATE_MAIN_MENU;
            necesitaRenderizare = true;
            uart_print("Returned to Main Menu");
        } else {
            uart_print("Already in Main Menu");
        }
    }
    else if (strncmp(serialBuffer, "MOVE X ", 7) == 0) {
        float val = custom_atof(serialBuffer + 7);
        float new_pos = current_x_mm + (val * 10.0); // convert cm to mm
        stop_requested = false;
        
        uart_print("Target: ");
        uart_print_int((long)new_pos);
        uart_print(" mm. Steps: ");
        uart_print_int((long)(new_pos * STEPS_PER_MM));
        
        moveTo(new_pos, current_y_mm);
        uart_print(" -> Done.");
    }
    else if (strncmp(serialBuffer, "MOVE Y ", 7) == 0) {
        float val = custom_atof(serialBuffer + 7);
        float new_pos = current_y_mm + (val * 10.0); // convert cm to mm
        stop_requested = false;
        
        uart_print("Target: ");
        uart_print_int((long)new_pos);
        uart_print(" mm. Steps: ");
        uart_print_int((long)(new_pos * STEPS_PER_MM));
        
        moveTo(current_x_mm, new_pos);
        uart_print(" -> Done.");
    }
    else if (strncmp(serialBuffer, "SELECT ", 7) == 0) {
        int val = atoi(serialBuffer + 7);
        
        if (stareCurenta == STATE_MAIN_MENU) {
            if (val == 1) {
                stareCurenta = STATE_TOUCH_DRAW;
                necesitaRenderizare = true;
                uart_print("Entered TOUCH DRAW");
            } else if (val == 2) {
                stareCurenta = STATE_SD_MENU;
                incarca_lista_sd_petit();
                selectieCurenta = 0;
                necesitaRenderizare = true;
                uart_print("Entered SD CARD MENU");
            } else if (val == 3) {
                stareCurenta = STATE_HOME;
                necesitaRenderizare = true;
                uart_print("Entered HOME SET");
            } else {
                uart_print("Invalid menu option");
            }
        }
        else if (stareCurenta == STATE_SD_MENU) {
            if (val >= 1 && val <= numarFisiereTotal) {
                int selectedIndex = val - 1;
                
                if (selectieCurenta == selectedIndex) {
                    start_plot(listaFisiere[selectieCurenta]);
                } else {
                    // Just select and highlight
                    selectieCurenta = selectedIndex;
                    necesitaRenderizare = true;
                    uart_print("File Selected");
                }
            } else {
                uart_print("Invalid file index!");
            }
        } else {
            uart_print("Select not applicable in this state");
        }
    } else {
        uart_print("Unknown Command");
    }
    
    // Clear buffer for the next command
    serialIndex = 0;
}

void checkSerial() {
    while (uart_available()) {
        char c = uart_read();
        
        // Handle Backspace/Delete
        if (c == '\b' || c == 0x7F) {
            if (serialIndex > 0) {
                serialIndex--;
                uart_print("\b \b"); // Erase character visually from the terminal
            }
        }
        // Handle Enter key
        else if (c == '\n' || c == '\r') {
            if (serialIndex > 0) {
                serialBuffer[serialIndex] = '\0';
                
                // Always check for STOP, even while moving
                if (strncmp(serialBuffer, "STOP", 4) == 0 || strncmp(serialBuffer, "stop", 4) == 0) {
                    stop_requested = true;
                    serialIndex = 0;
                    return;
                }

                if (!is_moving) {
                    uart_print("\r\n");   // Move to next line
                    processCommand();     // Execute the command
                    print_prompt();       // Print the prompt again
                } else {
                    uart_print("\r\n[Busy]\r\n> ");
                    serialIndex = 0; // Clear buffer
                }
            } else if (c == '\r') {
                // If they hit enter on an empty line, just give a fresh prompt
                print_prompt();
            }
        } 
        // Handle normal characters
        else {
            if (serialIndex < SERIAL_BUF_SIZE - 1) {
                serialBuffer[serialIndex++] = c;
                uart_print_char(c);   // Echo the character back to the user
            }
        }
    }
}


// ==========================================
// MAIN BREATHING LOOP
// ==========================================
int main(void) {
    uart_init(115200); // Strict AVR Serial init
    lcd_init();
    init_steppers();   // Initialize Motor Driver Pins
    
    // Initial State
    stareCurenta = STATE_MAIN_MENU;
    necesitaRenderizare = true;

    uart_print("Plotter UI Initialized.\r\n");
    uart_print("Commands: SELECT [1-10], BACK, STOP, MOVE X [cm], MOVE Y [cm]");
    print_prompt();

    while (1) {
        // 1. Process non-blocking inputs
        checkSerial();
        
        // 2. Render state changes seamlessly
        if (necesitaRenderizare) {
            necesitaRenderizare = false;
            
            switch(stareCurenta) {
                case STATE_MAIN_MENU:
                    randeaza_meniu_principal();
                    break;
                case STATE_SD_MENU:
                    randeaza_interfata_sd();
                    break;
                case STATE_TOUCH_DRAW:
                    randeaza_touch_draw();
                    break;
                case STATE_HOME:
                    randeaza_home();
                    break;
            }
        }
        
        // 3. Application Background Tasks (G-code Streaming)
        if (plottingActive && !stop_requested) {
            char gcode_line[64];
            if (read_gcode_line(gcode_line, sizeof(gcode_line))) {
                process_gcode_line(gcode_line);
            } else {
                uart_print("\r\nPlotting Completed!");
                print_prompt();
                plottingActive = false;
            }
        }
        
        // Allow the MCU to breathe briefly
        _delay_us(10);
    }
}