#ifndef STATE_SYS_TRINA_H
#define STATE_SYS_TRINA_H
#include "system.h"

#include  "main.h" 
extern uint8_t black_start ;
extern uint8_t black_end   ;
void SYS_Trina_State_Run(volatile SYS_State_ENUM *state, INT8U sys_num);
void SYS_Trina_State_Init(volatile SYS_State_ENUM *state, INT8U sys_num);
#endif