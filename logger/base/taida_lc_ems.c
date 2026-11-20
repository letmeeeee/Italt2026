/*LC通过封装台达PCS的控制点位和反馈点位来适配trina EMS*/
#include "main.h"
#include "taida_lc_ems.h"

extern INT8U BusType;

/* 读取输入寄存器指定位：bit 为 0~15（0 为最低位）。
 * 返回 0/1；地址或 bit 越界返回 -1。
 */
int LC_EMS_Get_Input_Bit(int addr, int bit)
{
    uint16_t value;

    if (addr < 0 || addr >= INPUT_SIZE || bit < 0 || bit > 15)
    {
        return -1;
    }

    value = (uint16_t)GET_INPUT(addr);
    return (int)((value >> bit) & 0x01U);
}
/*
 * 功能：
 * 根据输入地址进行映射转换
 *
 * 参数：
 *   n     -> 设备编号/索引
 *   addr  -> 原始地址
 *
 * 返回值：
 *   >=0   -> 转换后的地址
 *   -1    -> 地址不在映射范围内
 */
int LC_EMS_Convert_Address(int pcsnum, int addr)
{
    /* 组控制点基址：PCS_TAIDA_CTRL_REG[pcsnum].Base == 27000 + 300*pcsnum */
    int base = PCS_TAIDA_CTRL_REG[pcsnum].Base;

    /* 台达原始 +200 ~ +203 -> 点表 +103 SystemOnOffCmd */
    if (addr >= (base + 200) && addr <= (base + 203))
    {
        return PCS_TAIDA_CTRL_REG[pcsnum].SystemOnOffCmd;//开关机
    }

    /* 台达原始 +208 ~ +211 -> 点表 +53 ReactivePowerReference */
    if (addr >= (base + 208) && addr <= (base + 211))
    {
        return PCS_TAIDA_CTRL_REG[pcsnum].ReactivePowerReference;//无功功率
    }

    /* 台达原始 +212 ~ +215 -> 点表 +111 IslandVoltageReference */
    if (addr >= (base + 212) && addr <= (base + 215))
    {
        return PCS_TAIDA_CTRL_REG[pcsnum].IslandVoltageReference;//孤岛电压
    }

    /* 台达原始 +216 ~ +219 -> 点表 +112 IslandFrequencyReference */
    if (addr >= (base + 216) && addr <= (base + 219))
    {
        return PCS_TAIDA_CTRL_REG[pcsnum].IslandFrequencyReference;//孤岛频率
    }

    return -1;
}
/**
 * @brief 设置某组 PCS 的功率分配比例（多机分配）
 *
 * 根据总功率 sum 的正负（放电/充电）以及总线类型（D_BUS / 非 D_BUS），
 * 把 3 路分配比例 result[] 映射到不同的保持寄存器，并做去重后下发。
 *
 * @param pcsnum  PCS 组号，0 或 1（对应主机/从机两组）
 * @param sum     该组总功率，正=放电，负=充电
 * @param result  3 路分配比例（千分比），由 LC_EMS_Calc_Power_Percent 算出
 *
 * 寄存器成员约定（PCS_TAIDA_CTRL_REG[pcsnum]，即 27000 + 300*n，D_BUS 分支）：
 *   MasterBat2DischargePowerCoef(+46) / SlaveBat1DischargePowerCoef(+47)
 *   MasterBat2ChargePowerCoef(+48)    / SlaveBat1ChargePowerCoef(+49)
 *   MasterBat1DischargePowerCoef(+50) / MasterBat1ChargePowerCoef(+51)
 *   ActivePowerReference(+52)         ：本组总功率（有符号，正放负充）
 * 非 D_BUS 分支只用 +50 ~ +52（MasterBat1DischargePowerCoef / MasterBat1ChargePowerCoef /
 * ActivePowerReference）这 3 个寄存器。
 */
static void Set_Group_Power_Multi(int pcsnum, int32_t sum, int16_t result[3])
{
    int base;
     /* 每组 7 个寄存器的上一次快照，用于判断是否需要真正下发 */
    static unsigned short Hold[MAX_GROUP_NUM][7] = {0};
    bool isUpdate = false;
     /* 组号合法性检查，只有 0/1 两组 */
    if (pcsnum < 0 || pcsnum > 1)
    {
        return;
    }
    /* 本组寄存器基址：取点表控制点表项 Base（27000 / 27300），后续连续块按 Base+offset 访问 */
    base = PCS_TAIDA_CTRL_REG[pcsnum].Base;
    const int16_t master_default = SysConf_GetInfo()->singlePcsMaster ? 500 : POWER_DEFAULT_RATIO;
    const int16_t slave_default = SysConf_GetInfo()->singlePcsMaster ? 0 : POWER_DEFAULT_RATIO;
    if(BusType==D_BUS)
    {
        /* ================= D_BUS 分支：双套分配比例 ================= */
        /* 逻辑说明：
         *   放电(sum>0)：base+46/47/50 写入本次实际比例，base+48/49/51 复位为默认值
         *   充电(sum<0)：base+46/47/50 复位为默认值，base+48/49/51 写入本次实际比例
         *   待机(sum=0)：两套全部复位为默认值
         * 这样充放电使用不同的比例寄存器组，避免方向切换时残留旧值。
         */

        // if (sum == 0)
        // {
        //     return;
        // }

        

        if (sum < 0)
        {
             /* 充电：第一套比例复位默认，第二套写实际值 */
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1DischargePowerCoef, master_default);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat2DischargePowerCoef, master_default);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].SlaveBat1DischargePowerCoef, slave_default);

            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1ChargePowerCoef, result[0]);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat2ChargePowerCoef, result[1]);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].SlaveBat1ChargePowerCoef, result[2]);
        }
        else if(sum > 0)
        {
            /* 放电：第一套写实际值，第二套复位默认 */
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1DischargePowerCoef, result[0]);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat2DischargePowerCoef, result[1]);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].SlaveBat1DischargePowerCoef, result[2]);

            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1ChargePowerCoef, master_default);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat2ChargePowerCoef, master_default);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].SlaveBat1ChargePowerCoef, slave_default);
            
        }
        else
    	 {
            /* 0kW：两套全部复位为默认比例 */
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1DischargePowerCoef, master_default);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat2DischargePowerCoef, master_default);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].SlaveBat1DischargePowerCoef, slave_default);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1ChargePowerCoef, master_default);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat2ChargePowerCoef, master_default);
            SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].SlaveBat1ChargePowerCoef, slave_default);
    	 }
         /* 写入本组总功率（有符号，正放负充） */
        SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].ActivePowerReference, (int16_t)sum);
        /* ---- 去重：与上一次快照对比，7 个寄存器都没变就不下发 ---- */
        isUpdate = false;
        for (int i = 0; i < 7; i++)
        {
            if(GET_HOLD(base + 46+i)!=Hold[pcsnum][i])
            {
                isUpdate = true;
                Hold[pcsnum][i]=GET_HOLD(base + 46+i);
            }
        }
        if(false==isUpdate)
        {
            return;
        }
        LOG_INFO("Group%d 功率分配下发: sum=%d, 寄存器范围[%d~%d]",pcsnum, sum,
                 PCS_TAIDA_CTRL_REG[pcsnum].MasterBat2DischargePowerCoef,
                 PCS_TAIDA_CTRL_REG[pcsnum].ActivePowerReference);
         /* ---- 有变化时，置标志位，由上层任务统一组包下发 ---- */
        if (pcsnum == 0)
        {
            Group1_power_multi_Flag = true;
        }
        else
        {
            Group2_power_multi_Flag = true;
        }
    }
    else
    {
         /* ================= 非 D_BUS 分支：单套分配比例 ================= */
        /* 只用 base+50 / base+51 / base+52 三个寄存器：
         *   sum>0（放电）：base+50=实际比例，base+51=500（默认）
         *   sum<0（充电）：base+50=500（默认），base+51=实际比例
         *   sum=0（待机）：两者都 500
         */
      const INT16U default_ratio = SysConf_GetInfo()->singlePcsMaster ? 1000 : 500;
      if(sum>0)
      {
        SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1DischargePowerCoef, result[0]);
        SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1ChargePowerCoef,default_ratio);
      }
      else if(sum<0)
      {
        SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1DischargePowerCoef, default_ratio);
        SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1ChargePowerCoef, result[0]);
      }
      else
      {
          SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1DischargePowerCoef, default_ratio);
          SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1ChargePowerCoef, default_ratio);
      }
       /* 本组总功率 */
      SET_HOLD(PCS_TAIDA_CTRL_REG[pcsnum].ActivePowerReference, (int16_t)sum);
       /* ---- 去重：3 个寄存器都没变则不下发 ---- */
      isUpdate = false;
      for (int i = 0; i < 3; i++)
        {
            if(GET_HOLD(base + 50+i)!=Hold[pcsnum][i])
            {
                isUpdate = true;
                Hold[pcsnum][i]=GET_HOLD(base + 50+i);
            }
        }
        if(false==isUpdate)
        {
            return;
        }
        LOG_INFO("Group%d 功率分配下发: sum=%d, 寄存器范围[%d~%d]",pcsnum, sum,
                 PCS_TAIDA_CTRL_REG[pcsnum].MasterBat1DischargePowerCoef,
                 PCS_TAIDA_CTRL_REG[pcsnum].ActivePowerReference);
        /* ---- 逐个入队下发：非 D_BUS 用单寄存器写(F06) ---- */
        int num = (pcsnum>= 1) ? 2 : 0;
        for(INT8U i=0;i<3;i++)
        {
                PcsWriteReq pcsreq;
                pcsreq.addr     = base + 50+i;
                pcsreq.value    = GET_HOLD(base + 50+i);
                pcsreq.len      = 1;
                pcsreq.is_multi = false;/* false = 单寄存器写 F06 */
                /* 按地址去重入队：同一地址已有待发请求则用新值覆盖，保留最新 */
                Pcs_Write_Enqueue_Dedup_By_Addr(num, &pcsreq);//放入缓冲区中
                LOG_INFO("pcsnum:%d,addr:%d,value:%d", num, pcsreq.addr, pcsreq.value);
        }
    }
}

/**
 * @brief 计算某组 PCS 的功率分配比例
 *
 * 步骤：
 *   1. 从保持寄存器读取各路功率值（最多 4 路，受 bmsNum 限制）
 *   2. 根据充放电方向做限幅和状态检查（异常/禁充禁放时该路功率置 0）
 *   3. 累加得到组总功率 sum
 *   4. 按千分比算出各路比例 result[i]
 *   5. 调用 Set_Group_Power_Multi 下发
 *
 * @param pcsnum PCS 组号（0/1）
 */
void LC_EMS_Calc_Power_Percent(int pcsnum)
{
    int16_t value[4] = {0};     /* 各路原始功率 */
    int32_t sum = 0;            /* 限幅后各组功率之和 */
    int32_t sum_per = 0;        /* 限幅前各组功率之和（用于判断是否全 0） */
    int16_t result[4] = {0};          /* 各路千分比 */
    sysPara* sys_cfg = SysConf_GetInfo();
    if (pcsnum < 0 || pcsnum > 1) return;
    /* Keep physical group stride: two slots in S_BUS, four in D_BUS. */
    int bms_stride = Project_Bms_Group_Stride(sys_cfg);
    int base = PCS_TAIDA_CTRL_REG[pcsnum].Base;
    u16_conv DchgPower;         /* 放电允许功率 */
    u16_conv ChgPower;          /* 充电允许功率 */
    INT16U state;               /* 状态输入寄存器值 */
    sum = 0;
    sum_per = 0;
    
    /* 读取 4 路功率值，并对每路做限幅和状态检查 */
    for (int i = 0; (i < 4)&&(i < bms_stride); i++)
    {
        int bms_slot = pcsnum * bms_stride + i;
        if (sys_cfg->singlePcsMaster && !Project_Bms_Active(sys_cfg, bms_slot)) continue;
        value[i] = (int16_t)GET_HOLD(base + 204 + i);
        sum_per += value[i];
        if(value[i]>=0)//放电
        {
             /* ---- 放电方向 ---- */
            /* 检查 BMS 状态：非正常、非禁充则本路功率清零 */
            state = GET_INPUT(BMS_BAU_INPUT_REG[bms_slot].SystemRunMode);
            if((state!=0)&&(state!=1)) //不是正常，也不是禁充
            {
                value[i]=0;
                continue;
            }
            /* 检查 PCS 侧状态：非 0 视为不可用 */
            state = GET_INPUT(PCS_TAIDA_GROUP_INPUT_REG[pcsnum].Base + 200 + i);
            if(state!=0)
            {
                value[i]=0;
                continue;
            }
            /* 按放电允许功率限幅 */
            DchgPower.D16 = GET_INPUT(BMS_BAU_INPUT_REG[bms_slot].DchgPower);
            if((DchgPower.DS16>=0)&&(value[i]>DchgPower.DS16))
            {
                value[i]=DchgPower.DS16;
            }
            /* 按通道额定功率再次限幅 */
            if(value[i]>DC_CHAN_RATE_POWER)
            {
                value[i]=DC_CHAN_RATE_POWER;
            } 
        }
        else
        {
            /* ---- 充电方向 ---- */
            /* 检查 BMS 状态：非正常、非禁放则本路功率清零 */
            state = GET_INPUT(BMS_BAU_INPUT_REG[bms_slot].SystemRunMode);
            if((state!=0)&&(state!=2)) //不是正常，也不是禁放
            {
                value[i]=0;
                continue;
            }
            /* 检查 PCS 侧状态 */
            state = GET_INPUT(PCS_TAIDA_GROUP_INPUT_REG[pcsnum].Base + 200 + i);
            if(state!=0)
            {
                value[i]=0;
                continue;
            }
            /* 按充电允许功率限幅（注意负号） */
            ChgPower.D16 = GET_INPUT(BMS_BAU_INPUT_REG[bms_slot].ChgPower);
            if((ChgPower.DS16>=0)&&(value[i]<(-1)*ChgPower.DS16))
            {
                value[i]=(-1)*ChgPower.DS16;
            }
             /* 按通道额定功率再次限幅 */
            if(value[i]<(-1)*DC_CHAN_RATE_POWER)
            {
                value[i]=(-1)*DC_CHAN_RATE_POWER;
            } 
        }
        
    }
    /* 累加限幅后的功率 */
    for (int i = 0; (i < 4)&&(i < bms_stride); i++)
    {
        sum += value[i];
    }
       /* sum_per==0 表示所有路限幅后都是 0：
     *   如果之前也没下发过（Flag=false），直接返回；
     *   否则清 Flag 并继续下发一次"全 0"，让 PCS 知道功率已归零。 */
    if (sum != 0)
    {
        /* 计算千分比 */
        for (int i = 0; (i < 4)&&(i < bms_stride); i++)
        {
            result[i] = (int16_t)((value[i] * 1000) / sum);
        }
    }
    if (sum_per == 0)
    {
        if (!Group_power_per_Flag[pcsnum])
            return;

        Group_power_per_Flag[pcsnum] = false;
    }
    Set_Group_Power_Multi(pcsnum,sum,result);
}

/**
 * @brief 功率分配比例更新的对外入口
 * 先检查总有功设置和总无功设置联锁输入（1010/1011），只要有一个非 0 就禁止更新。
 * @param groupnum PCS 组号（0/1）
 */
void LC_EMS_Update_Power_Percent(int groupnum)
{

    /* 联锁检查：任一条件非 0 则退出，不更新功率分配 */
    if((GET_HOLD(1010)!=0)||(GET_HOLD(1011)!=0))
    {
        return;
    }
    
    LC_EMS_Calc_Power_Percent(groupnum);
}
// static void Set_Group_Power_Multi(int pcsnum, int32_t sum, int16_t result[3])
// {
//     int base;

//     if (pcsnum < 0 || pcsnum > 1)
//     {
//         return;
//     }

//     if (sum == 0)
//     {


//         return;
//     }

//     base = BASE_ADDR + STEP * pcsnum;

//     if (sum < 0)
//     {
//         SET_HOLD(base + 50, POWER_DEFAULT_RATIO);
//         SET_HOLD(base + 46, POWER_DEFAULT_RATIO);
//         SET_HOLD(base + 47, POWER_DEFAULT_RATIO);

//         SET_HOLD(base + 51, result[0]);
//         SET_HOLD(base + 48, result[1]);
//         SET_HOLD(base + 49, result[2]);
//     }
//     else
//     {
//         SET_HOLD(base + 50, result[0]);
//         SET_HOLD(base + 46, result[1]);
//         SET_HOLD(base + 47, result[2]);

//         SET_HOLD(base + 51, POWER_DEFAULT_RATIO);
//         SET_HOLD(base + 48, POWER_DEFAULT_RATIO);
//         SET_HOLD(base + 49, POWER_DEFAULT_RATIO);
//     }

//     SET_HOLD(base + 52, (int16_t)sum);

//     if (pcsnum == 0)
//     {
//         Group1_power_multi_Flag = true;
//     }
//     else
//     {
//         Group2_power_multi_Flag = true;
//     }
// }
// void LC_EMS_Calc_Power_Percent(int pcsnum)
// {
//     int16_t value[4];
//     int32_t sum = 0;
//     int16_t result[4];
//     int base = BASE_ADDR + STEP * pcsnum ;
//     if(BusType==D_BUS)
//     {
//     /* 读取4个功率值 */
//     for (int i = 0; i < 4; i++)
//     {
//         value[i] = (int16_t)GET_HOLD(base + 204 + i);

//         sum += value[i];
//         LOG_INFO("sum %d",sum);
//     }

//     /* 防止除0 */
//     if (sum == 0)
//     {
//         SET_HOLD(base + 50, POWER_DEFAULT_RATIO);
//         SET_HOLD(base + 46, POWER_DEFAULT_RATIO);
//         SET_HOLD(base + 47, POWER_DEFAULT_RATIO);
//         SET_HOLD(base + 51, POWER_DEFAULT_RATIO);
//         SET_HOLD(base + 48, POWER_DEFAULT_RATIO);
//         SET_HOLD(base + 49, POWER_DEFAULT_RATIO);

//     if (pcsnum == 0)
//     {
  
//         PcsWriteReq pcsreq;
//         pcsreq.addr     = 27000+300*0 + 52;
//         pcsreq.value    = 0;
//         pcsreq.len      = 1;
//         pcsreq.is_multi = false; 
//                                 // 去重，保留最新值
//         Pcs_Write_Enqueue_Dedup_By_Addr(0, &pcsreq);//放入缓冲区中
     
//     }
//     else if (pcsnum == 1 )
//     {
//         PcsWriteReq pcsreq;
//         pcsreq.addr     = 27000+300*1 + 52;
//         pcsreq.value    = 0;
//         pcsreq.len      = 1;
//         pcsreq.is_multi = false; 
//                                 // 去重，保留最新值
//         Pcs_Write_Enqueue_Dedup_By_Addr(2, &pcsreq);//放入缓冲区中



//     }

//         return;
//     }

//     /* 计算千分比 */
//     for (int i = 0; i < 4; i++)
//     {
//         result[i] = (int16_t)((value[i] * 1000) / sum);
//     }
//       Set_Group_Power_Multi(pcsnum,sum,result);

//     }
//     else
//     {
//       /* 读取2个功率值 */
//     for (int i = 0; i < 2; i++)
//     {
//         value[i] = (int16_t)GET_HOLD(base + 204 + i);

//         sum += value[i];
//     }

//     /* 防止除0 */
//     if (sum == 0)
//     {
//         SET_HOLD(base + 50, 500);
//         SET_HOLD(base + 51,500);
//         SET_HOLD(base + 52, sum);  
//     if (pcsnum == 0)
//     {
  
//         PcsWriteReq pcsreq;
//         pcsreq.addr     = 27000+300*0 + 52;
//         pcsreq.value    = 0;
//         pcsreq.len      = 1;
//         pcsreq.is_multi = false; 
//                                 // 去重，保留最新值
//         Pcs_Write_Enqueue_Dedup_By_Addr(0, &pcsreq);//放入缓冲区中
     
//     }
//     else if (pcsnum == 1 )
//     {
//         PcsWriteReq pcsreq;
//         pcsreq.addr     = 27000+300*1 + 52;
//         pcsreq.value    = 0;
//         pcsreq.len      = 1;
//         pcsreq.is_multi = false; 
//                                 // 去重，保留最新值
//         Pcs_Write_Enqueue_Dedup_By_Addr(2, &pcsreq);//放入缓冲区中



//     }
//         return;
//     }

//     /* 计算千分比 */
//     for (int i = 0; i < 2; i++)
//     {
//         result[i] = (int16_t)((value[i] * 1000) / sum);
//     }

//       if(sum>0)
//       {
//         SET_HOLD(base + 50, result[0]);
//         SET_HOLD(base + 51,500);
//         SET_HOLD(base + 52, sum);
//       }
//       else
//       {
//         SET_HOLD(base + 50, 500);
//         SET_HOLD(base + 51, result[0]);
//         SET_HOLD(base + 52, sum);
//       }
//     }



// }
static void Update_Bit_Status_By_One_Word(int base,
                                          int src_offset,
                                          int dst_offset_12,
                                          int dst_offset_13)
{
    uint16_t value;

    value = GET_INPUT(base + src_offset);

    /* bit12 -> dst_offset_12 */
    SET_INPUT(base + dst_offset_12, (value >> 12) & 0x01);

    /* bit13 -> dst_offset_13 */
    SET_INPUT(base + dst_offset_13, (value >> 13) & 0x01);
}

void Update_PCS_SEL_Goose_Status(int system_num)
{
    /* 17000 + 300*n + 68 -> 点表 SystemFaultWord3 */
    uint16_t value = GET_INPUT(PCS_TAIDA_GROUP_INPUT_REG[system_num].SystemFaultWord3);

    pcsSelGooseStatus[system_num].sel_ems = (value >> 9) & 0x01;
    pcsSelGooseStatus[system_num].sel_pt = (value >> 10) & 0x01;
    pcsSelGooseStatus[system_num].goose_sel = (value >> 11) & 0x01;
    pcsSelGooseStatus[system_num].pcs_goose = (value >> 12) & 0x01;
}

void Update_Input_Bit_Status(int system_num)
{
    int base = PCS_TAIDA_GROUP_INPUT_REG[system_num].Base;
    int sysRdyRef = 0, sysRdyRun = 0, acMcbState = 0;
    int slot = system_num * 2;
    /* 处理 17000 + 300*n + 60 -> SystemStateWord1 */
    sysRdyRun = LC_EMS_Get_Input_Bit(PCS_TAIDA_GROUP_INPUT_REG[system_num].SystemStateWord1, 2);

    sysRdyRef = LC_EMS_Get_Input_Bit(PCS_TAIDA_GROUP_INPUT_REG[system_num].SystemStateWord1, 3);

    acMcbState = LC_EMS_Get_Input_Bit(PCS_TAIDA_GROUP_INPUT_REG[system_num].SystemStateWord1, 13);

    /* 处理 17000 + 300*n + 61 -> SystemStateWord2 */
    Update_Bit_Status_By_One_Word(base, PCS_TAIDA_GROUP_INPUT_REG[system_num].SystemStateWord2, 200, 201);

    /* 处理 17000 + 300*n + 62 -> SystemStateWord3 */
    Update_Bit_Status_By_One_Word(base, PCS_TAIDA_GROUP_INPUT_REG[system_num].SystemStateWord3, 202, 203);

    /* 对应主机在线且PCS系统故障字bit9~bit12全为0时，通信才正常。
     * 第0组对应PCS0，第1组对应PCS2；断连清零故障字不能视为恢复。
     */
    Update_PCS_SEL_Goose_Status(system_num);
    int comm_normal = (Get_PCS_Comm(system_num * 2) == IsNoFault) &&
                      (pcsSelGooseStatus[system_num].StatusAll == 0);
    // LOG_INFO("system_num=%d,comm_normal=%d,pcsSelGooseStatus=%04X", system_num, comm_normal, pcsSelGooseStatus[system_num].StatusAll);
    for(int i=0;i<4;i++)
    {
        SET_INPUT(base + 208 + i, (sysRdyRun && acMcbState));
        SET_INPUT(base + 212 + i, GET_INPUT(PCS_TAIDA_GROUP_INPUT_REG[system_num].GridFrequency));
        SET_INPUT(base + 236 + i, sysRdyRef);
        SET_INPUT(base + 240 + i, comm_normal ? 1 : 0);
    }

    /*有符号值运算后写入*/
    for(int i=0;i<2;i++)
    {
        SET_INPUT(base + 228 + i, Taida_Read_Input(PCS_TAIDA_MODULE_INPUT_REG[slot].Base + 16 + i));
        SET_INPUT(base + 230 + i, Taida_Read_Input(PCS_TAIDA_MODULE_INPUT_REG[slot + 1].Base + 16 + i));
        SET_INPUT(base + 232 + i, (int16_t)Taida_Read_Input(PCS_TAIDA_MODULE_INPUT_REG[slot].GridReactivePower) / 2);
        SET_INPUT(base + 234 + i, (int16_t)Taida_Read_Input(PCS_TAIDA_MODULE_INPUT_REG[slot + 1].GridReactivePower) / 2);
    }
    for(int j=0;j<3;j++)
    {
        SET_INPUT(base + 216 + j, Taida_Read_Input(PCS_TAIDA_MODULE_INPUT_REG[slot].Base + 20 + j));
        SET_INPUT(base + 219 + j, Taida_Read_Input(PCS_TAIDA_MODULE_INPUT_REG[slot].Base + 20 + j));
        SET_INPUT(base + 222 + j, Taida_Read_Input(PCS_TAIDA_MODULE_INPUT_REG[slot + 1].Base + 20 + j));
        SET_INPUT(base + 225 + j, Taida_Read_Input(PCS_TAIDA_MODULE_INPUT_REG[slot + 1].Base + 20 + j));
    }




}
void Update_Slave_sysstate2(uint16_t Slave_sys_state2_value,uint16_t addr)
{
    uint16_t reg_value;

    /* 先读取17062原值，避免影响其他bit */
    reg_value = GET_INPUT(addr);

    /* 清除 bit8~bit13 */
    reg_value &= ~(0x3F << 8);

    /* 提取 Slave1_sys_state2 的 bit8~bit13 */
    reg_value |= (Slave_sys_state2_value & (0x3F << 8));

    /* 写回 */
    SET_INPUT(addr, reg_value);
}