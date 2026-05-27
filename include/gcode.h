#ifndef GCODE_H
#define GCODE_H

#include <stdbool.h>

float custom_atof(const char* str);
bool read_gcode_line(char* buf, int max_len);
void process_gcode_line(const char* line);

#endif
