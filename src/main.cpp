#include "globals.h"
#include "uart.h"
#include "lcd.h"
#include "stepper.h"
#include "ui.h"
#include "sd_handler.h"
#include "gcode.h"

// ==========================================
// GLOBALS INSTANTIATION
// ==========================================
SystemState currentState = STATE_MAIN_MENU;
bool needsRendering = true;
int currentSelection = 0;
bool plottingActive = false;
volatile bool stop_requested = false;
volatile bool is_moving = false;

char fileList[MAX_FILES][FILE_NAME_LEN];
uint8_t totalFiles = 0;
FATFS fs;
int sd_err = 0;
char err_stage[10] = "";

int microstepping_multiplier = 16; // Set to 1 for Full Step, 16 for Microstepping

volatile float current_x_mm = 0.0;
volatile float current_y_mm = 0.0;
volatile long current_step_x = 0;
volatile long current_step_y = 0;
uint16_t feedrate_delay_us = 1000;

// ISR Bresenham State Variables
volatile long isr_dx = 0;
volatile long isr_dy = 0;
volatile int isr_sx = 0;
volatile int isr_sy = 0;
volatile long isr_err = 0;
volatile long isr_steps_taken = 0;
volatile long isr_total_steps = 0;

// ==========================================
// MAIN LOOP
// ==========================================
int main(void) {
    uart_init(115200); 
    lcd_init();
    init_steppers();   
    init_timer1();     
    
    currentState = STATE_MAIN_MENU;
    needsRendering = true;

    uart_print("Plotter UI Initialized.\r\n");
    uart_print("Commands: SELECT [1-10], BACK, STOP, MOVE X [cm], MOVE Y [cm]");
    print_prompt();

    while (1) {
        checkSerial();
        
        if (needsRendering) {
            needsRendering = false;
            
            switch(currentState) {
                case STATE_MAIN_MENU:
                    render_main_menu();
                    break;
                case STATE_SD_MENU:
                    render_sd_menu();
                    break;
                case STATE_TOUCH_DRAW:
                    render_touch_draw();
                    break;
                case STATE_HOME:
                    render_home();
                    break;
            }
        }
        
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
    }

    return 0;
}