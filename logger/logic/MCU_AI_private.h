/*
 * File: MCU_AI_private.h
 *
 * Code generated for Simulink model 'MCU_AI'.
 *
 * Model version                  : 16.18
 * Simulink Coder version         : 25.2 (R2025b) 28-Jul-2025
 * C/C++ source code generated on : Mon Aug 24 15:14:57 2026
 *
 * Target selection: ert_shrlib.tlc
 * Embedded hardware selection: Generic->32-bit Embedded Processor
 * Emulation hardware selection:
 *    Differs from embedded hardware (MATLAB Host)
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */

#ifndef MCU_AI_private_h_
#define MCU_AI_private_h_
#include "rtwtypes.h"
#include "MCU_AI_types.h"
#include "MCU_AI.h"
#include "dsp_rt.h"  /* DSP System Toolbox general run time support functions */
#include "dspsrt_rt.h"         /* DSP System Toolbox run time support library */
#include "dsp_rt.h"  /* DSP System Toolbox general run time support functions */

extern real32_T rt_roundf_snf(real32_T u);
extern void MCU_A_SwitchCaseActionSubsystem(real32_T rtu_In1, real32_T rtu_In2,
  real32_T *rty_Out1, real32_T *rty_Out2);

#endif                                 /* MCU_AI_private_h_ */

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
