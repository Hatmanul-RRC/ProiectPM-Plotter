#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(uint32_t baud);
bool uart_available();
char uart_read();
void uart_print_char(char c);
void uart_print(const char* str);
void uart_print_int(long val);
void print_prompt();
void checkSerial();
void processCommand();

#endif
