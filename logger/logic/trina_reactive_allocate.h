#ifndef TRINA_REACTIVE_ALLOCATE_H
#define TRINA_REACTIVE_ALLOCATE_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define Trina_PCS_RATED_S_FULL_KVA 3556.0f
#define Trina_PCS_RATED_S_HALF_KVA (Trina_PCS_RATED_S_FULL_KVA / 2.0f)
#define Trina_PCS_RATED_S_ZERO_KVA 0.0f
#define Trina_PCS_BRANCH_NONE 0u
#define Trina_PCS_BRANCH_SINGLE 1u
#define Trina_PCS_BRANCH_DOUBLE 2u
#define Trina_PCS_POWER_COEF_BASE 1000u

/* standby means mapped state==2 (manual stop), not Q-capable standby. */
typedef struct {
    uint16_t branch1_run;
    uint16_t branch2_run;
    uint16_t branch1_standby;
    uint16_t branch2_standby;
    uint16_t branch1_fault;
    uint16_t branch2_fault;
} Trina_PCS_Branch_Status;

typedef struct {
    Trina_PCS_Branch_Status branch;
    float p_kw;  /* Planned signed kW, positive discharge, negative charge. */
    float rated_s_kva;
    uint16_t run_branch_num;
} Trina_PCS_Q_Info;

typedef struct {
    int16_t group1_total_p;
    int16_t group2_total_p;
    int16_t pcs1_p_set;
    int16_t pcs2_p_set;
    int16_t pcs3_p_set;
    int16_t pcs4_p_set;
    uint16_t group1_master_discharge_coef;
    uint16_t group1_slave_discharge_coef;
    uint16_t group1_master_charge_coef;
    uint16_t group1_slave_charge_coef;
    uint16_t group2_master_discharge_coef;
    uint16_t group2_slave_discharge_coef;
    uint16_t group2_master_charge_coef;
    uint16_t group2_slave_charge_coef;
} Trina_MV_Power_Distribution;

/* pcs*_q_cmd：signed16原始位模式，单位千分比，范围-1000～1000。
 * 固定计算：Q(kvar)*1000/3556，向0截断。
 * 所有q_max字段和balance_q_max仍为非负kvar。
 */
typedef struct {
    uint16_t pcs1_q_cmd;
    uint16_t pcs2_q_cmd;
    uint16_t pcs3_q_cmd;
    uint16_t pcs4_q_cmd;
    uint16_t pcs1_q_max;
    uint16_t pcs2_q_max;
    uint16_t pcs3_q_max;
    uint16_t pcs4_q_max;
    uint16_t group1_q_max;
    uint16_t group2_q_max;
    uint16_t balance_q_max;
    uint16_t pcs1_run_branch_num;
    uint16_t pcs2_run_branch_num;
    uint16_t pcs3_run_branch_num;
    uint16_t pcs4_run_branch_num;
    uint16_t group1_q_available_num;
    uint16_t group2_q_available_num;
} Trina_MV_Q_Result;

/* P为signed16原始值，单位kW；总Q为signed16原始值，单位kvar。
 * 此底层入口使用传入P值；自动读寄存器请调用下方PQ入口。
 */
void Trina_MV_ReactivePowerControl(uint16_t total_q_cmd,
    uint16_t pcs1_p_raw, uint16_t pcs2_p_raw,
    uint16_t pcs3_p_raw, uint16_t pcs4_p_raw,
    Trina_MV_Q_Result *result);

/* 自动读取12004、12704、13404、14104有功设置值。
 * 输出每台PCS无功千分比目标；不发送控制报文。
 * 组总P和比例字段仅兼容保留，本版本清零、不使用。
 */
void Trina_MV_PQ_Control(uint16_t total_q_cmd,
    Trina_MV_Power_Distribution *p_result, Trina_MV_Q_Result *q_result);

#ifdef __cplusplus
}
#endif
#endif
