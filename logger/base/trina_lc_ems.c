/*LC通过封装台达PCS的控制点位和反馈点位来适配trina EMS*/
#include "main.h"


#include <stdint.h>

#define PCS_GROUP_COUNT     2U  /* 共两组 */
#define PCS_PER_GROUP       2U  /* 每组两台PCS */

/*
 * 更新两组PCS，共四台PCS、八个支路。
 *
 * 源地址（点表 PCS_TRINA_MODULE_INPUT_REG[n-1]，即 2600 + 300*(n-1)）：
 *   第一组：n = 1、2
 *   第二组：n = 3、4
 *
 * 目标地址（点表 PCS_TRINA_GROUP_INPUT_REG[group]，即 17000 + 300*group）：
 *   group = 0：第一组
 *   group = 1：第二组
 *
 * 每组目标成员（均为 4 个直流通道的连续区间，Start + 支路索引）：
 *   DCChannelFault          +200～203：四个支路故障，0=无，1=有
 *   DCChannelAlarm          +204～207：四个支路告警，0=无，1=有
 *   DCChannelStatus         +208～211：四个支路状态
 *   DCChannelIslandFreq     +212～215：四个支路频率
 *   DCChannelGridVolt       +216～227：四个支路三相电压，每支路依次为R、S、T
 *   DCChannelActivePower    +228～231：四个支路功率
 *   DCChannelBlackStartRef  +236～239：四个支路黑启动状态反馈
 *
 * 每组支路顺序：
 *   第一台PCS支路1、第一台PCS支路2、
 *   第二台PCS支路1、第二台PCS支路2。
 *
 * 支路状态：
 *   0：故障停机，或运行时本支路有故障/告警，或其他状态
 *   1：PCS正在运行，且本支路无故障、无告警
 *   2：PCS手动停机
 *
 * 状态判断优先级：
 *   故障停机 > 手动停机 > 正在运行
 *
 * 同一台PCS的两个支路共用该PCS的频率和三相电压。
 * 功率分别读取该PCS的 +141、+148。
 * 电压、频率、功率均原值映射，不进行倍率或符号转换。
 *
 * 注意：GET_INPUT(addr) 需要支持直接赋值。
 */
void UpdateTrinaPCStoEMS(void)
{
    uint16_t group;
    uint16_t pcs;
    uint16_t branch;
    uint16_t blackstart_state;
    uint16_t droop_pq;
    for (group = 0U; group < PCS_GROUP_COUNT; group++)
    {
        /* 目标组：PCS_TRINA_GROUP_INPUT_REG[group]，即 17000 / 17300 */

        for (pcs = 0U; pcs < PCS_PER_GROUP; pcs++)
        {
            uint16_t srcIdx;

            uint16_t faultReg;
            uint16_t alarmReg;
            uint16_t runReg;

            uint16_t gridFreq;
            uint16_t rVoltage;
            uint16_t sVoltage;
            uint16_t tVoltage;
            uint16_t branchPower[2];

            /*
             * 第一组：n=1、2（点表 #1、#2）
             * 第二组：n=3、4（点表 #3、#4）
             */
            srcIdx = group * PCS_PER_GROUP + pcs;

            /* 读取当前PCS的故障、告警及运行状态 */
            faultReg = GET_INPUT(PCS_TRINA_MODULE_INPUT_REG[srcIdx].MsCtrlDspFaultTable4);
            alarmReg = GET_INPUT(PCS_TRINA_MODULE_INPUT_REG[srcIdx].MsCtrlDspWarningTable1);
            runReg   = GET_INPUT(PCS_TRINA_MODULE_INPUT_REG[srcIdx].SystemRunStatus);
            /*黑启动状态反馈*/
            blackstart_state = GET_INPUT(PCS_TRINA_MODULE_INPUT_REG[srcIdx].BlackStartState);
            droop_pq = GET_HOLD(PCS_TRINA_HOLD_REG[srcIdx].Droop_PQ);
            /* 读取当前PCS的电网频率、三相电压 */
            gridFreq = GET_INPUT(PCS_TRINA_MODULE_INPUT_REG[srcIdx].GridFrequencyPCC);
            rVoltage = GET_INPUT(PCS_TRINA_MODULE_INPUT_REG[srcIdx].GridVoltageRSPCC);
            sVoltage = GET_INPUT(PCS_TRINA_MODULE_INPUT_REG[srcIdx].GridVoltageSTPCC);
            tVoltage = GET_INPUT(PCS_TRINA_MODULE_INPUT_REG[srcIdx].GridVoltageTRPCC);

            /* 分别读取两个支路的功率（点表：电池功率 J1 / J2） */
            branchPower[0] = GET_INPUT(PCS_TRINA_MODULE_INPUT_REG[srcIdx].BatteryPowerJ1);
            branchPower[1] = GET_INPUT(PCS_TRINA_MODULE_INPUT_REG[srcIdx].BatteryPowerJ2);

            for (branch = 0U; branch < 2U; branch++)
            {
                uint16_t fault;
                uint16_t alarm;
                uint16_t state;
                uint16_t index;
                uint16_t voltageAddr;

                /* 提取当前支路的故障和告警 */
                if (branch == 0U)
                {
                    /* 支路1：故障bit0，告警bit3 */
                    fault = faultReg & 0x01U;
                    alarm = (alarmReg >> 3U) & 0x01U;
                }
                else
                {
                    /* 支路2：故障bit4，告警bit4 */
                    fault = (faultReg >> 4U) & 0x01U;
                    alarm = (alarmReg >> 4U) & 0x01U;
                }

                /* 默认状态为0 */
                state = 0U;

                if ((runReg & (1U << 2U)) != 0U)
                {
                    /* PCS故障停机 */
                    state = 0U;
                }
                else if ((runReg &
                         ((1U << 1U) | (1U << 3U))) != 0U)
                {
                    /* PCS手动停机 */
                    state = 2U;
                }
                else if ((runReg & (1U << 0U)) != 0U)
                {
                    /* PCS正在运行 */
                    if ((fault == 0U) && (alarm == 0U))
                    {
                        state = 1U;  /* 本支路正常运行 */
                    }
                    else
                    {
                        state = 0U;  /* 本支路有故障或告警 */
                    }
                }

                /*
                 * 每组内部支路索引：
                 * 0：第一台PCS支路1
                 * 1：第一台PCS支路2
                 * 2：第二台PCS支路1
                 * 3：第二台PCS支路2
                 */
                index = pcs * 2U + branch;

                /* 映射故障、告警、支路状态 */
                SET_INPUT(PCS_TRINA_GROUP_INPUT_REG[group].DCChannelFault.Start + index, fault);
                SET_INPUT(PCS_TRINA_GROUP_INPUT_REG[group].DCChannelAlarm.Start + index, alarm);
                SET_INPUT(PCS_TRINA_GROUP_INPUT_REG[group].DCChannelStatus.Start + index, state);
                SET_INPUT(PCS_TRINA_GROUP_INPUT_REG[group].DCChannelBlackStartRef.Start + index,
                          blackstart_state);
                /* 映射频率：同一台PCS两个支路使用相同值 */
                SET_INPUT(PCS_TRINA_GROUP_INPUT_REG[group].DCChannelIslandFreq.Start + index, gridFreq);
                /*映射下垂转PQ模式：同一台PCS两个支路使用相同值*/
                SET_HOLD(PCS_TRINA_GROUP_HOLD_REG[group].Droop_pq.Start + index, droop_pq);
                /*
                 * 映射三相电压，每个支路占3个寄存器：
                 * index=0：+216、217、218
                 * index=1：+219、220、221
                 * index=2：+222、223、224
                 * index=3：+225、226、227
                 */
                voltageAddr = PCS_TRINA_GROUP_INPUT_REG[group].DCChannelGridVolt.Start
                              + index * 3U;

                SET_INPUT(voltageAddr,rVoltage);
                SET_INPUT(voltageAddr + 1U,sVoltage);
                SET_INPUT(voltageAddr + 2U,tVoltage);

                /* 映射当前支路功率 */
                SET_INPUT(PCS_TRINA_GROUP_INPUT_REG[group].DCChannelActivePower.Start + index,
                          branchPower[branch]);
            }
        }
    }
}
INT16U TrinaPCSLCtoEMSaddr(INT16U recvAddr)
{
    /*
     * 直流通道开/关命令（点表 PCS_TRINA_CTRL_REG[n].DCChannelXOnOffCmd，
     * 即 27000 + 300*n + 200~203）映射到对应 PCS 的开机指令
     * （点表 PCS_TRINA_HOLD_REG[m].PowerOnCommand，即 12000 + 700*m + 1）。
     * 每台 PCS 的前两个通道归一台 PCS，后两个通道归下一台 PCS。
     * 注：case 标签不能用 const 数组取值，故改用 if 判断。
     */
    if ((recvAddr == PCS_TRINA_CTRL_REG[0].DCChannel1OnOffCmd) ||
        (recvAddr == PCS_TRINA_CTRL_REG[0].DCChannel2OnOffCmd))
    {
        return PCS_TRINA_HOLD_REG[0].PowerOnCommand;
    }

    if ((recvAddr == PCS_TRINA_CTRL_REG[0].DCChannel3OnOffCmd) ||
        (recvAddr == PCS_TRINA_CTRL_REG[0].DCChannel4OnOffCmd))
    {
        return PCS_TRINA_HOLD_REG[1].PowerOnCommand;
    }

    if ((recvAddr == PCS_TRINA_CTRL_REG[1].DCChannel1OnOffCmd) ||
        (recvAddr == PCS_TRINA_CTRL_REG[1].DCChannel2OnOffCmd))
    {
        return PCS_TRINA_HOLD_REG[2].PowerOnCommand;
    }

    if ((recvAddr == PCS_TRINA_CTRL_REG[1].DCChannel3OnOffCmd) ||
        (recvAddr == PCS_TRINA_CTRL_REG[1].DCChannel4OnOffCmd))
    {
        return PCS_TRINA_HOLD_REG[3].PowerOnCommand;
    }

    return recvAddr;
}