#include "stepper.h"
#include "globals.h"
#include "uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>

#define INVERT_X_DIR false
#define INVERT_Y_DIR true
#define BASE_STEPS_PER_MM 5 

#define MOTOR_PORT PORTA
#define MOTOR_DDR  DDRA
#define X_DIR_PIN  PA0
#define X_STEP_PIN PA1
#define Y_DIR_PIN  PA2
#define Y_STEP_PIN PA3

void init_steppers() {
    MOTOR_DDR |= (1 << X_DIR_PIN) | (1 << X_STEP_PIN) | (1 << Y_DIR_PIN) | (1 << Y_STEP_PIN);
    MOTOR_PORT &= ~((1 << X_DIR_PIN) | (1 << X_STEP_PIN) | (1 << Y_DIR_PIN) | (1 << Y_STEP_PIN));
}

void update_timer_speed() {
    cli();
    TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10)); // Clear prescaler bits
    
    if (microstepping_multiplier == 16) {
        // 1/16th Microstepping -> 4000 Hz
        OCR1A = 499; 
        TCCR1B |= (1 << CS11); // Prescaler 8
    } else {
        // Full Step -> 250 Hz
        OCR1A = 999; 
        TCCR1B |= (1 << CS11) | (1 << CS10); // Prescaler 64
    }
    sei();
}

void init_timer1() {
    cli(); 
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;
    TCCR1B |= (1 << WGM12); // CTC mode
    TIMSK1 |= (1 << OCIE1A);
    sei(); 
    
    update_timer_speed();
}

ISR(TIMER1_COMPA_vect) {
    if (!is_moving) return;

    if (stop_requested) {
        is_moving = false;
        current_x_mm = (float)current_step_x / (BASE_STEPS_PER_MM * microstepping_multiplier);
        current_y_mm = (float)current_step_y / (BASE_STEPS_PER_MM * microstepping_multiplier);
        return;
    }

    if (isr_steps_taken < isr_total_steps) {
        long e2 = 2 * isr_err;
        bool step_x = false;
        bool step_y = false;

        if (e2 >= -isr_dy) {
            isr_err -= isr_dy;
            current_step_x += isr_sx;
            step_x = true;
        }
        if (e2 <= isr_dx) {
            isr_err += isr_dx;
            current_step_y += isr_sy;
            step_y = true;
        }

        if (step_x) MOTOR_PORT |= (1 << X_STEP_PIN);
        if (step_y) MOTOR_PORT |= (1 << Y_STEP_PIN);
        
        _delay_us(5); 
        
        MOTOR_PORT &= ~((1 << X_STEP_PIN) | (1 << Y_STEP_PIN));

        isr_steps_taken++;
    } else {
        is_moving = false;
        current_x_mm = (float)current_step_x / (BASE_STEPS_PER_MM * microstepping_multiplier);
        current_y_mm = (float)current_step_y / (BASE_STEPS_PER_MM * microstepping_multiplier);
    }
}

void moveTo(float target_x_mm, float target_y_mm) {
    while (is_moving) {
        checkSerial();
    }
    
    long current_steps_per_mm = BASE_STEPS_PER_MM * microstepping_multiplier;
    long target_step_x = (long)(target_x_mm * current_steps_per_mm);
    long target_step_y = (long)(target_y_mm * current_steps_per_mm);

    long dx = labs(target_step_x - current_step_x);
    long dy = labs(target_step_y - current_step_y);
    int sx = current_step_x < target_step_x ? 1 : -1;
    int sy = current_step_y < target_step_y ? 1 : -1;

    long total_steps = (dx > dy) ? dx : dy;
    if (total_steps == 0) return; 

    bool dir_x_high = (sx > 0);
    if (INVERT_X_DIR) dir_x_high = !dir_x_high;
    
    bool dir_y_high = (sy > 0);
    if (INVERT_Y_DIR) dir_y_high = !dir_y_high;

    if (dir_x_high) MOTOR_PORT |= (1 << X_DIR_PIN);
    else MOTOR_PORT &= ~(1 << X_DIR_PIN);

    if (dir_y_high) MOTOR_PORT |= (1 << Y_DIR_PIN);
    else MOTOR_PORT &= ~(1 << Y_DIR_PIN);

    cli(); 
    isr_dx = dx;
    isr_dy = dy;
    isr_sx = sx;
    isr_sy = sy;
    isr_err = (dx > dy ? dx : -dy) / 2;
    isr_steps_taken = 0;
    isr_total_steps = total_steps;
    is_moving = true; 
    sei();
    
    while (is_moving) {
        checkSerial();
    }
}
