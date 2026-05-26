#ifndef _DISKIO_DEFINED
#define _DISKIO_DEFINED

#include <avr/io.h>
#include "pff.h"

/* Tipuri de date necesare pentru Petit FatFs */
typedef BYTE DSTATUS;

typedef enum {
    RES_OK = 0,
    RES_ERROR,
    RES_NOTRDY,
    RES_PARERR
} DRESULT;

/* Status bit flags */
#define STA_NOINIT  0x01

/* Prototipuri funcții */
DSTATUS disk_initialize (void);
DRESULT disk_readp (BYTE* buff, DWORD sector, UINT offset, UINT count);

#endif