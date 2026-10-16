#include <stdint.h>
#include <math.h>
#include <string.h>

#include "main.h"





static float Trina_MV_U16ToSignedFloat(uint16_t raw)
{
    return (raw <= 32767u) ? (float)raw : (float)((int32_t)raw - 65536L);
}


/* 直接读取各PCS有功设置值，单位kW，按有符号16位解释。
 * 兼容保留的组总有功和比例字段清零，不再参与计算。
 */
static void Trina_MV_CalcPCSActivePower(Trina_MV_Power_Distribution *r)
{
    if (!r) return;
    memset(r, 0, sizeof(*r));
    r->pcs1_p_set = (int16_t)Trina_MV_U16ToSignedFloat(GET_HOLD(12004));
    r->pcs2_p_set = (int16_t)Trina_MV_U16ToSignedFloat(GET_HOLD(12704));
    r->pcs3_p_set = (int16_t)Trina_MV_U16ToSignedFloat(GET_HOLD(13404));
    r->pcs4_p_set = (int16_t)Trina_MV_U16ToSignedFloat(GET_HOLD(14104));
}


static void Trina_PCS_ParseBranchStatus(uint16_t state_addr,
                                 uint16_t fault_addr,
                                 Trina_PCS_Branch_Status *branch)
{
    uint16_t state1 = (uint16_t)GET_INPUT(state_addr);
    uint16_t state2 = (uint16_t)GET_INPUT(state_addr + 1u);

    branch->branch1_run = (state1 == 1u) ? 1u : 0u;
    branch->branch2_run = (state2 == 1u) ? 1u : 0u;
    branch->branch1_standby = (state1 == 2u) ? 1u : 0u;
    branch->branch2_standby = (state2 == 2u) ? 1u : 0u;
    branch->branch1_fault = (uint16_t)GET_INPUT(fault_addr);
    branch->branch2_fault = (uint16_t)GET_INPUT(fault_addr + 1u);
}

/* Effective capacity: two running branches=3556, one=1778, none=0 kVA. */
static float Trina_PCS_GetRatedPowerByBranch(Trina_PCS_Branch_Status *branch,uint16_t *run_branch_num)
{
    uint16_t num;

    if ((branch == 0) || (run_branch_num == 0))
    {
        return Trina_PCS_RATED_S_ZERO_KVA;
    }

    num =branch->branch1_run+branch->branch2_run;

    *run_branch_num = num;

    if (num >= Trina_PCS_BRANCH_DOUBLE)
    {
        return Trina_PCS_RATED_S_FULL_KVA;
    }

    if (num == Trina_PCS_BRANCH_SINGLE)
    {
        return Trina_PCS_RATED_S_HALF_KVA;
    }

    return Trina_PCS_RATED_S_ZERO_KVA;
}



static float Trina_PCS_CalcQMax(
        float p_kw,
        float rated_s_kva)
{
    float p_abs;

    if (p_kw < 0.0f)
    {
        p_abs = -p_kw;
    }
    else
    {
        p_abs = p_kw;
    }

    if (rated_s_kva <= 0.0f)
    {
        return 0.0f;
    }

    /*
     * 有功已经达到或超过当前有效视在功率，
     * 无剩余无功能力，同时避免sqrt负数。
     */
    if (p_abs >= rated_s_kva)
    {
        return 0.0f;
    }

    return sqrtf(
        rated_s_kva * rated_s_kva
        -
        p_abs * p_abs);
}


/* Two PCS per group. Target stride is per GROUP, not per PCS. */
static void Trina_MV_Q_ReadPCSInfo(
        uint16_t master1_p_raw, uint16_t slave1_p_raw,
        uint16_t master2_p_raw, uint16_t slave2_p_raw,
        Trina_PCS_Q_Info *master1, Trina_PCS_Q_Info *slave1,
        Trina_PCS_Q_Info *master2, Trina_PCS_Q_Info *slave2)
{
    Trina_PCS_Q_Info *info[4] = {master1, slave1, master2, slave2};
    const uint16_t states[4] = {17208u, 17210u, 17508u, 17510u};
    const uint16_t faults[4] = {17200u, 17202u, 17500u, 17502u};
    uint16_t power[4] = {master1_p_raw, slave1_p_raw,
                         master2_p_raw, slave2_p_raw};
    uint16_t i;

    if (!master1 || !slave1 || !master2 || !slave2)
        return;

    for (i = 0u; i < 4u; ++i)
    {
        Trina_PCS_ParseBranchStatus(states[i], faults[i], &info[i]->branch);
        info[i]->rated_s_kva = Trina_PCS_GetRatedPowerByBranch(
            &info[i]->branch, &info[i]->run_branch_num);
        info[i]->p_kw = Trina_MV_U16ToSignedFloat(power[i]);
    }
}

/* 将组目标按两台PCS的整数Qmax比例分配，第二台取余量保证总和。
 * 组目标已限制在能力之和以内，因此各台指令均不超过其Qmax。
 */
static void Trina_AllocateGroupQ(int32_t target, uint16_t cap1,
    uint16_t cap2, uint16_t *out1, uint16_t *out2)
{
    uint32_t cap = (uint32_t)cap1 + cap2;
    uint32_t magnitude;
    uint32_t first;
    uint32_t second;
    int32_t sign = (target < 0) ? -1 : 1;
    *out1 = 0u;
    *out2 = 0u;
    if (!cap || !target) return;
    magnitude = (uint32_t)((target < 0) ? -target : target);
    if (magnitude > cap) magnitude = cap;
    first = (uint32_t)(((uint64_t)magnitude * cap1) / cap);
    second = magnitude - first;
    *out1 = (uint16_t)(sign * (int32_t)first);
    *out2 = (uint16_t)(sign * (int32_t)second);
}

/* 最终输出转换：固定以3556为分母，保留正负号，向0截断。
 * 32位乘法防止16位溢出。单支路运行也不改变此分母。
 */
static uint16_t Trina_QKvarToPermille(uint16_t q_raw)
{
    int32_t q = (q_raw <= 32767u) ? (int32_t)q_raw : (int32_t)q_raw - 65536L;
    int32_t pm = q * 100L / 3556L;
    if (pm > 100L) pm = 100L;
    else if (pm < -100L) pm = -100L;
    return (uint16_t)pm;
}

void Trina_MV_ReactivePowerControl(uint16_t total_q_cmd,
    uint16_t pcs1_p_raw, uint16_t pcs2_p_raw,
    uint16_t pcs3_p_raw, uint16_t pcs4_p_raw,
    Trina_MV_Q_Result *result)
{
    Trina_PCS_Q_Info pcs[4] = {0};
    uint16_t cap[4];
    uint16_t group1, group2, balance;
    uint16_t i;
    int32_t target;
    if (!result) return;
    memset(result, 0, sizeof(*result));
    Trina_MV_Q_ReadPCSInfo(pcs1_p_raw, pcs2_p_raw, pcs3_p_raw, pcs4_p_raw,
        &pcs[0], &pcs[1], &pcs[2], &pcs[3]);
    for (i = 0u; i < 4u; ++i)
    {
        /* 按每台能力向下取整后分配，确保整数指令不越限。 */
        cap[i] = (uint16_t)Trina_PCS_CalcQMax(pcs[i].p_kw, pcs[i].rated_s_kva);
    }
    result->pcs1_q_max = cap[0];
    result->pcs2_q_max = cap[1];
    result->pcs3_q_max = cap[2];
    result->pcs4_q_max = cap[3];
    result->pcs1_run_branch_num = pcs[0].run_branch_num;
    result->pcs2_run_branch_num = pcs[1].run_branch_num;
    result->pcs3_run_branch_num = pcs[2].run_branch_num;
    result->pcs4_run_branch_num = pcs[3].run_branch_num;
    result->group1_q_available_num = (uint16_t)(
        (pcs[0].run_branch_num > 0u) + (pcs[1].run_branch_num > 0u));
    result->group2_q_available_num = (uint16_t)(
        (pcs[2].run_branch_num > 0u) + (pcs[3].run_branch_num > 0u));
    group1 = (uint16_t)(cap[0] + cap[1]);
    group2 = (uint16_t)(cap[2] + cap[3]);
    balance = (group1 < group2) ? group1 : group2;
    result->group1_q_max = group1;
    result->group2_q_max = group2;
    result->balance_q_max = balance;
    SET_INPUT(17031, balance);
    SET_INPUT(17331, balance);
    SET_INPUT(117, (uint16_t)(2u * balance));
    /* 任意组无能力时两组均为零。 */
    if (!balance) return;
    /* 总Q为奇数时向零截断，保持两组相等。 */
    target = (int32_t)Trina_MV_U16ToSignedFloat(total_q_cmd) / 2;
    if (target > (int32_t)balance) target = balance;
    else if (target < -(int32_t)balance) target = -(int32_t)balance;
    Trina_AllocateGroupQ(target, cap[0], cap[1],
        &result->pcs1_q_cmd, &result->pcs2_q_cmd);
    Trina_AllocateGroupQ(target, cap[2], cap[3],
        &result->pcs3_q_cmd, &result->pcs4_q_cmd);

    /* 仅最终四台控制目标转千分比；所有q_max、能力回写仍为kvar。 */
    result->pcs1_q_cmd = Trina_QKvarToPermille(result->pcs1_q_cmd);
    result->pcs2_q_cmd = Trina_QKvarToPermille(result->pcs2_q_cmd);
    result->pcs3_q_cmd = Trina_QKvarToPermille(result->pcs3_q_cmd);
    result->pcs4_q_cmd = Trina_QKvarToPermille(result->pcs4_q_cmd);
   // LOG_INFO("pcs1_q_cmd:%d, pcs2_q_cmd:%d, pcs3_q_cmd:%d, pcs4_q_cmd:%d", result->pcs1_q_cmd, result->pcs2_q_cmd, result->pcs3_q_cmd, result->pcs4_q_cmd);
}

void Trina_MV_PQ_Control(uint16_t total_q_cmd,
    Trina_MV_Power_Distribution *p_result, Trina_MV_Q_Result *q_result)
{
    if (!p_result || !q_result) return;
    Trina_MV_CalcPCSActivePower(p_result);
    Trina_MV_ReactivePowerControl(total_q_cmd,
        (uint16_t)p_result->pcs1_p_set, (uint16_t)p_result->pcs2_p_set,
        (uint16_t)p_result->pcs3_p_set, (uint16_t)p_result->pcs4_p_set,
        q_result);
}
