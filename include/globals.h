#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include "pff.h" // For FATFS and sd_err

enum SystemState {
    STATE_MAIN_MENU,
    STATE_SD_MENU,
    STATE_TOUCH_DRAW,
    STATE_HOME
};

extern SystemState currentState;
extern bool needsRendering;
extern int currentSelection;
extern bool plottingActive;
extern volatile bool stop_requested;
extern volatile bool is_moving;

// SD Data
#define MAX_FILES 10
#define FILE_NAME_LEN 13
extern char fileList[MAX_FILES][FILE_NAME_LEN];
extern uint8_t totalFiles;
extern FATFS fs;
extern int sd_err;
extern char err_stage[10];

// Kinetics Config
extern int microstepping_multiplier;

// Motion Tracking
extern volatile float current_x_mm;
extern volatile float current_y_mm;
extern volatile long current_step_x;
extern volatile long current_step_y;
extern uint16_t feedrate_delay_us;

// ISR Bresenham State Variables
extern volatile long isr_dx;
extern volatile long isr_dy;
extern volatile int isr_sx;
extern volatile int isr_sy;
extern volatile long isr_err;
extern volatile long isr_steps_taken;
extern volatile long isr_total_steps;

#endif
