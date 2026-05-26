#include "diskio.h"
#include <util/delay.h>
#include <avr/io.h>

/* 
 * CRITICAL FIX:
 * Since you are using an UNO TFT shield on a Mega2560 (evident from the LCD data pins 8,9,2,3,4,5,6,7),
 * the SD card is physically hardwired to Arduino Pins 10, 11, 12, 13.
 * On a Mega2560, Pins 11, 12, 13 are NOT the hardware SPI pins! 
 * (Hardware SPI on Mega is 50, 51, 52).
 * Additionally, Pin 4 (PG5) was being used for LCD Data, so it cannot be SD CS!
 * 
 * Therefore, we MUST use Software SPI (Bit-Banging) on pins PB4 (D10), PB5 (D11), PB6 (D12), PB7 (D13).
 */

#define SD_CS_PORT   PORTB
#define SD_CS_DDR    DDRB
#define SD_CS_PIN    PB4  // Pin 10

#define SPI_PORT     PORTB
#define SPI_DDR      DDRB
#define SPI_MOSI     PB5  // Pin 11
#define SPI_MISO     PB6  // Pin 12
#define SPI_SCK      PB7  // Pin 13
#define SPI_MISO_PIN PINB

/* Definitions for MMC/SDC command */
#define CMD0    (0x40+0)    /* GO_IDLE_STATE */
#define CMD1    (0x40+1)    /* SEND_OP_COND (MMC) */
#define ACMD41  (0xC0+41)   /* SEND_OP_COND (SDC) */
#define CMD8    (0x40+8)    /* SEND_IF_COND */
#define CMD16   (0x40+16)   /* SET_BLOCKLEN */
#define CMD17   (0x40+17)   /* READ_SINGLE_BLOCK */
#define CMD55   (0x40+55)   /* APP_CMD */
#define CMD58   (0x40+58)   /* READ_OCR */
#define CMD12   (0x40+12)   /* STOP_TRANSMISSION */

/* Card type flags (CardType) */
#define CT_MMC      0x01
#define CT_SD1      0x02
#define CT_SD2      0x04
#define CT_SDC      (CT_SD1|CT_SD2)
#define CT_BLOCK    0x08

static BYTE CardType;
static BYTE spi_slow = 1;

static void spi_init(void) {
    /* Set MOSI, SCK and CS as output, MISO as input */
    SPI_DDR |= (1 << SPI_MOSI) | (1 << SPI_SCK);
    SPI_DDR &= ~(1 << SPI_MISO);
    SPI_PORT |= (1 << SPI_MISO); // pull-up on MISO
    
    SD_CS_DDR |= (1 << SD_CS_PIN);
    SD_CS_PORT |= (1 << SD_CS_PIN); /* CS high */
    
    SPI_PORT &= ~(1 << SPI_SCK); /* SCK low */
}

static BYTE spi_transfer(BYTE data) {
    BYTE res = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (data & 0x80) SPI_PORT |= (1 << SPI_MOSI);
        else SPI_PORT &= ~(1 << SPI_MOSI);
        data <<= 1;
        
        if (spi_slow) _delay_us(2);
        
        SPI_PORT |= (1 << SPI_SCK);
        
        if (spi_slow) _delay_us(2);
        
        res <<= 1;
        if (SPI_MISO_PIN & (1 << SPI_MISO)) res |= 1;
        
        SPI_PORT &= ~(1 << SPI_SCK);
    }
    return res;
}

static void select_sd(void) {
    SD_CS_PORT &= ~(1 << SD_CS_PIN);
}

static void deselect_sd(void) {
    SD_CS_PORT |= (1 << SD_CS_PIN);
    spi_transfer(0xFF); /* Dummy clock to force DO hi-z */
}

/* Wait for ready */
static BYTE wait_ready (void) {
    BYTE res;
    WORD timeout = 0x0FFF;
    do {
        res = spi_transfer(0xFF);
    } while (res != 0xFF && --timeout);
    return res;
}

static BYTE send_cmd (BYTE cmd, DWORD arg) {
    BYTE n, res;
    if (cmd & 0x80) { /* ACMD<n> is the command sequense of CMD55-CMD<n> */
        cmd &= 0x7F;
        res = send_cmd(CMD55, 0);
        if (res > 1) return res;
    }
    /* Select the card and wait for ready */
    deselect_sd();
    select_sd();
    if (wait_ready() != 0xFF) return 0xFF;
    
    /* Send command packet */
    spi_transfer(cmd);
    spi_transfer((BYTE)(arg >> 24));
    spi_transfer((BYTE)(arg >> 16));
    spi_transfer((BYTE)(arg >> 8));
    spi_transfer((BYTE)arg);
    n = 0x01; /* Dummy CRC + Stop */
    if (cmd == CMD0) n = 0x95; /* Valid CRC for CMD0(0) */
    if (cmd == CMD8) n = 0x87; /* Valid CRC for CMD8(0x1AA) */
    spi_transfer(n);
    
    /* Receive command response */
    if (cmd == CMD12) spi_transfer(0xFF); /* Skip a stuff byte when stop reading */
    n = 10; /* Wait for a valid response in timeout of 10 attempts */
    do {
        res = spi_transfer(0xFF);
    } while ((res & 0x80) && --n);
    return res;
}

DSTATUS disk_initialize(void) {
    BYTE n, cmd, ty, ocr[4];
    WORD timeout;
    
    spi_slow = 1; /* Initialization needs low SPI speed */
    spi_init();
    
    _delay_ms(10);
    
    for (n = 10; n; n--) spi_transfer(0xFF); /* 80 dummy clocks with CS=H */
    
    ty = 0;
    if (send_cmd(CMD0, 0) == 1) { /* Enter Idle state */
        if (send_cmd(CMD8, 0x1AA) == 1) { /* SDv2 */
            for (n = 0; n < 4; n++) ocr[n] = spi_transfer(0xFF);
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) { /* The card can work at vdd range of 2.7-3.6V */
                for (timeout = 10000; timeout && send_cmd(ACMD41, 1UL << 30); timeout--) {
                    _delay_us(100);
                }
                if (timeout && send_cmd(CMD58, 0) == 0) { /* Check CCS bit */
                    for (n = 0; n < 4; n++) ocr[n] = spi_transfer(0xFF);
                    ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;
                }
            }
        } else { /* SDv1 or MMC */
            if (send_cmd(ACMD41, 0) <= 1) {
                ty = CT_SD1; cmd = ACMD41; /* SDv1 */
            } else {
                ty = CT_MMC; cmd = CMD1; /* MMCv3 */
            }
            for (timeout = 10000; timeout && send_cmd(cmd, 0); timeout--) {
                _delay_us(100);
            }
            if (!timeout || send_cmd(CMD16, 512) != 0) /* Set R/W block length to 512 */
                ty = 0;
        }
    }
    CardType = ty;
    deselect_sd();
    
    if (ty) {
        spi_slow = 0; /* Switch to fast SPI speed */
        return 0;
    }
    
    return STA_NOINIT;
}

DRESULT disk_readp (BYTE* buff, DWORD sector, UINT offset, UINT count) {
    DRESULT res;
    WORD rc;
    
    if (!(CardType & CT_BLOCK)) sector *= 512;
    
    res = RES_ERROR;
    if (send_cmd(CMD17, sector) == 0) {
        /* Wait for data packet */
        rc = 30000;
        do {
            _delay_us(10);
        } while (spi_transfer(0xFF) == 0xFF && --rc);
        
        if (rc) {
            /* Skip leading bytes */
            WORD bc = 512 - offset - count;
            while (offset--) spi_transfer(0xFF);
            
            /* Read data */
            if (buff) {
                while (count--) *buff++ = spi_transfer(0xFF);
            } else {
                while (count--) spi_transfer(0xFF);
            }
            
            /* Skip trailing bytes and CRC */
            while (bc--) spi_transfer(0xFF);
            spi_transfer(0xFF); spi_transfer(0xFF);
            
            res = RES_OK;
        }
    }
    
    deselect_sd();
    return res;
}