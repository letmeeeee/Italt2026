#ifndef PCS_GLOBALS_H
#define PCS_GLOBALS_H

#include <stdbool.h>

#define PCS_MAX_NUM 10

extern bool PCS_Write_Flag[PCS_MAX_NUM];
extern int PCSWrite_Lenth[PCS_MAX_NUM];
extern int PCSaddr[PCS_MAX_NUM];
extern int PCSvalue[PCS_MAX_NUM];
extern int PCSactivepower[PCS_MAX_NUM];
extern int PCSPF[PCS_MAX_NUM];

#endif
