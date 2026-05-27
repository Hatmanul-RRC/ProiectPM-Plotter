#include "uart.h"
#include "globals.h"
#include "stepper.h"
#include "ui.h"
#include "sd_handler.h"
#include "gcode.h"
#include <avr/io.h>
#include <string.h>
#include <stdlib.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#define SERIAL_BUF_SIZE 64
char serialBuffer[SERIAL_BUF_SIZE];
uint8_t serialIndex = 0;

void uart_init(uint32_t baud) {
    uint16_t ubrr = F_CPU / 8 / baud - 1;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr);
    UCSR0A = (1 << U2X0); 
    UCSR0B = (1 << RXEN0) | (1 << TXEN0); 
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); 
}

bool uart_available() {
    return (UCSR0A & (1 << RXC0));
}

char uart_read() {
    return UDR0;
}

void uart_print_char(char c) {
    while (!(UCSR0A & (1 << UDRE0))); 
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

void uart_print_int(long val) {
    char buf[12];
    ltoa(val, buf, 10);
    uart_print(buf);
}

void processCommand() {
    serialBuffer[serialIndex] = '\0';
    
    for (uint8_t i = 0; i < serialIndex; i++) {
        if (serialBuffer[i] >= 'a' && serialBuffer[i] <= 'z') {
            serialBuffer[i] -= 32;
        }
    }
    
    if (strncmp(serialBuffer, "STOP", 4) == 0) {
        stop_requested = true;
        plottingActive = false;
        uart_print("EMERGENCY STOP EXECUTED!");
    }
    else if (strncmp(serialBuffer, "BACK", 4) == 0) {
        if (currentState != STATE_MAIN_MENU) {
            currentState = STATE_MAIN_MENU;
            needsRendering = true;
            uart_print("Returned to Main Menu");
        } else {
            uart_print("Already in Main Menu");
        }
    }
    else if (strncmp(serialBuffer, "MOVE X ", 7) == 0) {
        float val = custom_atof(serialBuffer + 7);
        float new_pos = current_x_mm + (val * 10.0);
        stop_requested = false;
        
        uart_print("Target: ");
        uart_print_int((long)new_pos);
        uart_print(" mm.");
        
        moveTo(new_pos, current_y_mm);
        uart_print(" -> Done.");
    }
    else if (strncmp(serialBuffer, "MOVE Y ", 7) == 0) {
        float val = custom_atof(serialBuffer + 7);
        float new_pos = current_y_mm + (val * 10.0);
        stop_requested = false;
        
        uart_print("Target: ");
        uart_print_int((long)new_pos);
        uart_print(" mm.");
        
        moveTo(current_x_mm, new_pos);
        uart_print(" -> Done.");
    }
    else if (strncmp(serialBuffer, "SELECT ", 7) == 0) {
        int val = atoi(serialBuffer + 7);
        
        if (currentState == STATE_MAIN_MENU) {
            if (val == 1) {
                currentState = STATE_SD_MENU;
                load_sd_file_list();
                currentSelection = 0;
                needsRendering = true;
                uart_print("Entered SD CARD MENU");
            } else if (val == 2) {
                current_step_x = 0;
                current_step_y = 0;
                current_x_mm = 0.0;
                current_y_mm = 0.0;
                uart_print("HOME POSITION SET (0,0)");
            } else if (val == 3) {
                uart_print("GOING HOME...");
                moveTo(0.0, 0.0);
                uart_print(" -> Done");
            } else {
                uart_print("Invalid menu option");
            }
        }
        else if (currentState == STATE_SD_MENU) {
            if (val >= 1 && val <= totalFiles) {
                int selectedIndex = val - 1;
                
                if (currentSelection == selectedIndex) {
                    start_plot(fileList[currentSelection]);
                } else {
                    currentSelection = selectedIndex;
                    needsRendering = true;
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
    
    serialIndex = 0;
}

void checkSerial() {
    while (uart_available()) {
        char c = uart_read();
        
        if (c == '\b' || c == 0x7F) {
            if (serialIndex > 0) {
                serialIndex--;
                uart_print("\b \b");
            }
        }
        else if (c == '\n' || c == '\r') {
            if (serialIndex > 0) {
                serialBuffer[serialIndex] = '\0';
                
                if (strncmp(serialBuffer, "STOP", 4) == 0 || strncmp(serialBuffer, "stop", 4) == 0) {
                    stop_requested = true;
                    serialIndex = 0;
                    return;
                }

                if (!is_moving) {
                    uart_print("\r\n");
                    processCommand();
                    print_prompt();
                } else {
                    uart_print("\r\n[Busy]\r\n> ");
                    serialIndex = 0;
                }
            } else if (c == '\r') {
                print_prompt();
            }
        } 
        else {
            if (serialIndex < SERIAL_BUF_SIZE - 1) {
                serialBuffer[serialIndex++] = c;
                uart_print_char(c);
            }
        }
    }
}
