#include "gcode.h"
#include "globals.h"
#include "stepper.h"
#include "pff.h"

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
            break; 
        }
        str++;
    }
    return neg ? -res : res;
}

bool read_gcode_line(char* buf, int max_len) {
    int i = 0;
    UINT bw;
    char c;
    while (i < max_len - 1) {
        if (pf_read(&c, 1, &bw) != FR_OK || bw == 0) {
            if (i == 0) return false; 
            break;
        }
        if (c == '\n' || c == '\r') {
            if (i > 0) break; 
            continue; 
        }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return true;
}

void process_gcode_line(const char* line) {
    if ((line[0] == 'G' && line[1] == '0') || (line[0] == 'G' && line[1] == '1')) {
        float next_x = current_x_mm;
        float next_y = current_y_mm;

        const char* ptr = line + 2;
        while (*ptr) {
            if (*ptr == 'X') {
                next_x = custom_atof(ptr + 1);
            }
            else if (*ptr == 'Y') {
                next_y = custom_atof(ptr + 1);
            }
            ptr++;
        }
        
        moveTo(next_x, next_y);
    }
}
