#include "sd_handler.h"
#include "globals.h"
#include "uart.h"
#include "pff.h"
#include <string.h>

void load_sd_file_list(void) {
    totalFiles = 0;

    sd_err = pf_mount(&fs);
    if (sd_err != FR_OK) {
        strcpy(err_stage, "MOUNT");
        return; 
    }

    DIR dir;
    sd_err = pf_opendir(&dir, "/");
    if (sd_err != FR_OK) {
        sd_err = pf_opendir(&dir, ""); 
        if (sd_err != FR_OK) {
            strcpy(err_stage, "OPENDIR");
            return;
        }
    }

    FILINFO fno;
    while (totalFiles < MAX_FILES) {
        sd_err = pf_readdir(&dir, &fno);
        if (sd_err != FR_OK) {
            strcpy(err_stage, "READDIR");
            break;
        }
        if (fno.fname[0] == 0) {
            break;
        }

        if (!(fno.fattrib & AM_DIR)) {
            char* fullName = fno.fname;
            if (strstr(fullName, ".GCO") != NULL || 
                strstr(fullName, ".gco") != NULL || 
                strstr(fullName, ".GCODE") != NULL || 
                strstr(fullName, ".gcode") != NULL) {
                
                strncpy(fileList[totalFiles], fullName, FILE_NAME_LEN - 1);
                fileList[totalFiles][FILE_NAME_LEN - 1] = '\0';
                totalFiles++;
            }
            else if (totalFiles == 0 && strlen(fullName) > 0) {
                strncpy(fileList[totalFiles], fullName, FILE_NAME_LEN - 1);
                fileList[totalFiles][FILE_NAME_LEN - 1] = '\0';
                totalFiles = 1;
                break;
            }
        }
    }
}

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
