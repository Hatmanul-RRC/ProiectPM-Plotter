#ifndef PFCONF_DEF
#define PFCONF_DEF 8088 /* Revision ID esențial pentru validare în pff.h */

/* Function Configurations (0:Disable, 1:Enable) */
#define PF_USE_READ     1   /* pf_read() function */
#define PF_USE_DIR      1   /* Activează pf_opendir() și pf_readdir() */
#define PF_USE_LSEEK    0   
#define PF_USE_WRITE    0   

/* System Configurations */
#define PF_FS_FAT12     0
#define PF_FS_FAT16     1   
#define PF_FS_FAT32     1   /* Activează suport FAT32 pentru carduri moderne */

#define PF_USE_LCC      0   
#define PF_CODE_PAGE    437

#endif