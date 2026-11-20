
#include "main.h"
#include "timed_check.h"

volatile RegistersModel RegM;
volatile RankModel RankM;
// * 前面的 volatile 修饰寄存器数据，`*` 后面的 const 表示指针初始化后不能改指向。
volatile RegistersModel *const pRegM = &RegM;
volatile RankModel *const pRankM = &RankM;

/* *****************************************trina写寄存器反向映射**************************************/
int Trina_HOLD_ADDRESS(int pcs_index, int index)
{
    int base = 27000 + 300 * (pcs_index - 1);
    int offset = index - base;

    if (offset >= 0 && offset < 300)
    {
        if (offset >= 0 && offset < 1)         return 3004; // offset 0
        else if (offset >= 3 && offset < 4)    return 3001; // offset 3
        else if (offset >= 4 && offset < 5)    return 3000; // offset 4
        else if (offset >= 5 && offset < 6)    return 3002; // offset 5
        else if (offset >= 6 && offset < 7)    return 3003; // offset 6
        else if (offset >= 7 && offset < 8)    return 3005; // offset 7
        else if (offset >= 8 && offset < 9)    return 3006; // offset 8

        else if (offset >= 9 && offset < 29)   return 3007 + (offset - 9);   // [3007, 3027)
        else if (offset >= 29 && offset < 36)  return 3050 + (offset - 29);  // [3050, 3057)

        else return -2; // offset不在已知范围
    }

    return -1; // index非法
}
/**************************************END******************************************************** */
 INT32S G2pro_ADDR_FROM_INDEX(INT8U bms_index, INT32S index)
{
    if (bms_index == 0) return -1;                 
    INT32S base = 300 * (INT32S)(bms_index - 1);   
    INT32S offset = index - base;                     
    // 一键并网开关～清除PCS控制失效
    if (offset >= 35001 && offset <= 35004) {
        return 0x3001 + (offset - 35001);
    }
   //设置Rack启用/停用
    else if (offset >= 35005 && offset <= 35028) {
        return 0x3005 + (offset - 35005);
    }
   //本地/远程控制切换/一键复归
    else if (offset >= 35029 && offset <= 35031) {
        return 0x3045 + (offset - 35029);
    }
    //NTP 时区～BAU-EMS 通讯心跳
    else if (offset >= 35032 && offset <= 35039) {
        return 0x522D + (offset - 35032);
    }

    return -1; // 未命中
}
/**********************************协能BMS，根据EMS或者web发送的地址获得BMS编号******************************** */
 static int BANK_base_addresses[] = {
     35001, 35002, 35003, 35004, 35005, 35006, 35007, 35008, 35009, 35010,
     35011, 35012, 35013, 35014, 35015, 35016, 35017, 35018, 35019, 35020,
     35021, 35022, 35023, 35024, 35025, 35026, 35027, 35028, 35029, 35030,
     35031, 35032, 35033, 35034, 35035, 35036, 35037, 35038, 35039
     
 };
 
 #define NUM_BASES_BANK (sizeof(BANK_base_addresses)/sizeof(BANK_base_addresses[0]))
 //获取BANK_NUM
int Find_BANK_N(int addr, int *base_out) {
     for (int i = 0; i < NUM_BASES_BANK; i++) {
         int base = BANK_base_addresses[i];
         if ((addr - base) % 300 == 0) {
             int n = (addr - base) / 300 + 1;
             if (n >= 1 && n <= 10) {
                 *base_out = base-35000;
                 return n;
             }
         }
     }
     return -1; // 未找到
 }

 /*******************************************END*************************************************************** */

/**********************************trinaPCS，根据EMS或者web发送的地址获得pcs编号******************************** */
 static int trinaPCS_base_addresses[] = {
     27009, 27010, 27011, 27012, 27013, 27014, 27015, 27016, 27017,
     27018, 27019, 27020, 27021, 27022, 27023, 27024,
     27025, 27026, 27027, 27028,
     27029, 27030, 27031, 27032,
     27033, 27034, 27035, 27000,27007,27003,27004,27006,27005,27008

 };
 
 #define NUM_BASES_trinaPCS (sizeof(trinaPCS_base_addresses)/sizeof(trinaPCS_base_addresses[0]))
 //获取BANK_NUM
int Find_trina_N(int addr, int *base_out) {
     for (int i = 0; i < NUM_BASES_trinaPCS; i++) {
         int base = trinaPCS_base_addresses[i];
         if ((addr - base) % 300 == 0) {
             int n = (addr - base) / 300 + 1;
             if (n >= 1 && n <= 10) {
                 *base_out = base-27000;
                 return n;
             }
         }
     }
     return -1; // 未找到
 }

 /*******************************************END*************************************************************** */

 /* *****************************************Taida写寄存器反向映射**************************************/
/**
 * @brief 根据PCS索引和地址索引计算Modbus寄存器地址
 * 
 * @param pcs_index PCS设备索引（从1开始）
 * @param index 输入地址索引
 * @return INT32S 计算得到的Modbus寄存器地址，若无效则返回-1
 * 
 * @details 该函数实现地址映射功能，将输入索引转换为Modbus寄存器地址：
 *          - 处理特殊地址区间（12000-12025, 12300-12325, 12600-12625, 12900-12925）
 *          - 处理单点映射（27000+103/105/106/46/47/48/49）
 *          - 处理区间映射（27000+107..112, 27000+113..118）
 *          - 支持多个PCS设备的地址偏移计算
 */
INT32S Taida_Addr_From_Index(INT8U pcs_index, INT32S index)
{
    /* The writer uses group indices 1/2 for physical master slots 0/2. */
    if (SysConf_GetInfo()->singlePcsMaster && (pcs_index == 1 || pcs_index == 2)) {
        INT32S local = index - (15000 + 600 * (pcs_index - 1));
        if (local >= 0 && local < 24) return 200 + local;
        if (local >= 24 && local < 26) return 240 + local - 24;
        if (local >= 26 && local < 33) return 10 + local - 26;
    }
    if (pcs_index == 0) return -1;                  
    INT32S base = 300 * (INT32S)(pcs_index - 1);  
    INT32S offset = index - base;                      

    if((index>=12000)&&(index<=12032))
    {

    INT32S base = 300 * (INT32S)(pcs_index - 1);   
    INT32S offset = index - base;      
   
    if (offset >= 12000 + 0 && offset < 12000 + 0 + 24) {
        return 40201-40001 + (offset - (12000 + 0));
    }

    
    if (offset >= 12000 + 24 && offset < 12000 + 24 + 2) {
        return 40241-40001 + (offset - (12000 + 24));
    }

    if (offset >= 12000 + 26 && offset < 12000 + 33) {
        return 40011-40001 + (offset - (12000 + 26));
    }

    }
     else if((index>=12300)&&(index<=12332))
    {

    INT32S base = 300 * (INT32S)(pcs_index - 4);  
    INT32S offset = index - base;      
   
    if (offset >= 12000 + 0 && offset < 12000 + 0 + 24) {
        return 40201-40001 + (offset - (12000 + 0));
    }

    
    if (offset >= 12000 + 24 && offset < 12000 + 24 + 2) {
        return 40241-40001 + (offset - (12000 + 24));
    }

    if (offset >= 12000 + 26 && offset < 12000 + 33) {
        return 40011-40001 + (offset - (12000 + 26));
    }

    }   
     else if((index>=12600)&&(index<=12632))
    {

    INT32S base = 300 * (INT32S)(pcs_index);  
    INT32S offset = index - base;      
 
   
    if (offset >= 12000 + 0 && offset < 12000 + 0 + 24) {
        return 40201-40001 + (offset - (12000 + 0));
    }

   
    if (offset >= 12000 + 24 && offset < 12000 + 24 + 2) {
        return 40241-40001 + (offset - (12000 + 24));
    }

    if (offset >= 12000 + 26 && offset < 12000 + 33) {
        return 40011-40001 + (offset - (12000 + 26));
    }

    } 
     else if((index>=12900)&&(index<=12932))
    {

    INT32S base = 300 * (INT32S)(pcs_index - 3);  
    INT32S offset = index - base;      
    
    if (offset >= 12000 + 0 && offset < 12000 + 0 + 24) {
        return 40201-40001 + (offset - (12000 + 0));
    }

    
    if (offset >= 12000 + 24 && offset < 12000 + 24 + 2) {
        return 40241-40001 + (offset - (12000 + 24));
    }
    if (offset >= 12000 + 26 && offset < 12000 + 33) {
        return 40011-40001 + (offset - (12000 + 26));
    }
    } 
    // === 单点映射（与正向宏中的三个等值判断一致） ===
    if (offset == 27000 + 103) return 40103-40001;          // System ON/OFF Cmd
    if (offset == 27000 + 50) return 40105-40001;          // Master Bat1 Discharge Power Coef
    if (offset == 27000 + 51) return 40108-40001;          // Master Bat1 Charge Power Coef

    if (offset == 27000 + 46) return 40106-40001;          
    if (offset == 27000 + 47) return 40107-40001;          
    if (offset == 27000 + 48) return 40109-40001;          
    if (offset == 27000 + 49) return 40110-40001;  
    if (offset == 27000 + 52) return 40111-40001;          
    if (offset == 27000 + 53) return 40112-40001; 
    
    // === 区间一：40111..40116  → local = 27000+107 .. 27000+112 ===
    if (offset >= 27000 + 109 && offset < 27000 + 109 + 4) {
        return 40113-40001 + (offset - (27000 + 109));
    }

    // === 区间二：40229..40234  → local = 27000+113 .. 27000+18 ===
    if (offset >= 27000 + 113 && offset < 27000 + 113 + 6) {
        return 40229-40001 + (offset - (27000 + 113));
    }

    return -1; // 未命中
}

INT32S Trina_Addr_From_Index(INT8U pcs_index, INT32S index)
{
    if (pcs_index == 0) return -1;                  
    INT32S base = 700 * (INT32S)(pcs_index - 1);  
    INT32S offset = index - base;                      
    //并联模式～支路充电分配系数
    if (offset >= 12000 + 0 && offset <= 12000 + 36) {
        return 3000 + (offset - (12000 + 0));
    }
     if (offset == 12000 + 37) {
        return 3000 + (offset - 12037 + 750);
    }
    //升级信号～PCS出厂设置
    if (offset >= 12000 + 77 && offset <= 12000 + 78) {
        return 3077 + (offset - (12000 + 77));
    }
  //IP地址设置
    if (offset >= 12000 + 86 && offset <=12000 + 89) {
        return 3086 + (offset - (12000 + 86));
    }
  //安规参数设置
    if (offset >= 12000 + 94 && offset <=12000 + 116) {
        return 3094 + (offset - (12000 + 94));
    }
  //安规参数设置
    if (offset >= 12000 + 118 && offset <=12000 + 201) {
        return 3118 + (offset - (12000 + 118));
    }
    
  //安规参数设置
    if (offset >= 12000 + 204 && offset <=12000 + 409) {
        return 3204 + (offset - (12000 + 204));
    }
  //安规参数设置
    if (offset >= 12000 + 440 && offset <=12000 + 461) {
        return 3800 + (offset - (12000 + 440));
    }
    return -1; // 未命中
}
/* 取指定位（返回 0 或 1）*/
static inline INT16U bit_u16(INT16U value, INT8U bit)
{
    return (INT16U)((value >> bit) & 0x1u);
}

/**
 * @brief  根据地址(99/100/101/103)把16位值按位写入到指定的保持寄存器区
 *         规则：
 *         - addr==99 : bit0..bit15 -> SET_HOLD(27000 + i)
 *         - addr==100: bit0..bit15 -> SET_HOLD(27016 + i)
 *         - addr==101: 仅 bit8 -> SET_HOLD(27040), bit9 -> SET_HOLD(27041)
 *         - addr==103: bit0..bit15 -> SET_HOLD(27042 + i)
 *
 * @param addr  地址（只处理 99/100/101/102）
 * @param value 16位值
 * @return 0 成功；-1 地址不支持
 */
int write_bits_by_addr(INT16U addr, INT16U value,INT16U pcs_num)
{
    INT16U base;
    INT16U i;

    switch (addr) {
    case 99:
      
        SET_HOLD(27000+pcs_num*300, bit_u16(value, 0));
        SET_HOLD(27001+pcs_num*300, bit_u16(value, 1));
        // SET_HOLD(27004+pcs_num*300, bit_u16(value, 4));//PCS心跳在写心跳线程中直接写入到SET_HOLD中
        SET_HOLD(27005+pcs_num*300, bit_u16(value, 5));



        return 0;

    case 100:
        base = 27016+pcs_num*300;
        for (i = 0; i < 16; ++i) {
            SET_HOLD((INT16U)(base + i), bit_u16(value, (INT8U)i));
        }
        return 0;

    case 101:
        /* 仅写 bit8 和 bit9 */
        SET_HOLD(27040+pcs_num*300, bit_u16(value, 8));
        SET_HOLD(27041+pcs_num*300, bit_u16(value, 9));
        return 0;

    case 103:
        base = 27042+pcs_num*300;
        for (i = 0; i < 4; ++i) {
            SET_HOLD((INT16U)(base + i), bit_u16(value, (INT8U)i));
        }
        return 0;

    default:
        /* 不支持的地址 */
        return -1;
    }
}




  /*******************************************END*************************************************************** */
  /**********************************TaidaPCS，根据EMS或者web发送的地址获得pcs编号******************************** */
 static int TaidaPCS_base_addresses[] = {
     27036, 27037, 
     27043, 27044, 27045, 27046, 27047, 27048, 27049,27050,
     27051, 27052, 27053, 27054, 27055, 
     27056, 27057, 27058, 27059, 
     27035, 27034, 27033, 27032,27031,27015,27000,27001,27038,27024,

 };
 
 #define NUM_BASES_TaidaPCS (sizeof(TaidaPCS_base_addresses)/sizeof(TaidaPCS_base_addresses[0]))
 //获取PCS_NUM
int Find_Taida_N(int addr, int *base_out) {
     for (int i = 0; i < NUM_BASES_TaidaPCS; i++) {
         int base = TaidaPCS_base_addresses[i];
         if ((addr - base) % 300 == 0) {
             int n = (addr - base) / 300 + 1;
             if (n >= 1 && n <= 10) {
                 *base_out = base-27000;
                 return n;
             }
         }
     }
     return -1; // 未找到
 }

 /*******************************************END*************************************************************** */

  /**********************************cem9000******************************** ************************************/
#define CEM9000_SRC_BASE      1000
#define CEM9000_SRC_LAST      1007
#define EMS_DST_BASE      27600
#define BITS_PER_REG  16



/*  目标地址 = 27300 + (src_reg-1000)*16 + bit */
static inline INT16U map_dst_addr(INT16U src_reg, INT8U bit)
{
    return (INT16U)(EMS_DST_BASE + (src_reg - CEM9000_SRC_BASE) * BITS_PER_REG + bit);
}

static inline INT16U map_hold_dst_addr(INT8U bit)
{
    return (INT16U)(33200 + bit);
}

int CEM9000_INPUT(INT16U address, INT16U value)
{
    if (address < CEM9000_SRC_BASE || address > CEM9000_SRC_LAST) return -10;  // 超范围保护

    for (INT8U bit = 0; bit < BITS_PER_REG; ++bit) {
        INT16U v = (INT16U)((value >> bit) & 0x1u);     // 取该位的 0/1
        INT16U dst = map_dst_addr(address, bit);             // 计算目标地址
    

        SET_INPUT(dst, v);
    
    }
    return 0;
}


/* Finland YX: source 1000..1008 carries 140 defined bit positions. Unlike
 * Italy's contiguous export, Finland spares are skipped and AR51 points
 * are exported to the separate 25969..25994 block in MV-EMS V0.27. */
int CEM9000_FINLAND_INPUT(INT16U address, INT16U value)
{
    INT8U bit;
    int point, index;

    if (address < 1000 || address > 1008) return -10; // 超范围保护
    for (bit = 0; bit < 16; ++bit) {
        point = (address - 1000) * 16 + bit;
        if (point >= 140) break;
        index = Cem9000_Finland_YX_INDEX(point);
        if (index >= 0) {
            SET_INPUT(index, (value >> bit) & 0x1u);
        }
    }
    return 0;
}
/* 与现有 CEM9000_INPUT/CEM9000_FINLAND_INPUT 相同的逐位处理方式。 */
int CEM9000_SWEDEN_INPUT(INT16U address, INT16U value)
{
    INT8U bit;
    int point, index;

    if (address < 1000 || address > 1007) return -10;
    for (bit = 0; bit < 16; ++bit) {
        point = (address - 1000) * 16 + bit;
        if (point >= 118) break;
        index = Cem9000_Sweden_YX_INDEX(point);
        if (index >= 0) {
            SET_INPUT(index, (value >> bit) & 0x1u);
        }
    }
    return 0;
}

/* UK source 1000..1028 has 454 bit positions; spare bits are skipped. */
int CEM9000_UK_INPUT(INT16U address, INT16U value)
{
    INT8U bit;
    int point, index;

    if (address < 1000 || address > 1028) return -10;
    for (bit = 0; bit < 16; ++bit) {
        point = (address - 1000) * 16 + bit;
        if (point >= 454) break;
        index = Cem9000_UK_YX_INDEX(point);
        if (index >= 0) {
            SET_INPUT(index, (value >> bit) & 0x1u);
        }
    }
    return 0;
}

int CEM9000_HOLD(INT16S value)
{


    for (INT8U bit = 0; bit < 8; ++bit) {
        INT16U v = (INT16U)((value >> bit) & 0x1u);     // 取该位的 0/1
        INT16U dst = map_hold_dst_addr(bit);             // 计算目标地址
        SET_HOLD(dst, v);
     
    }
    return 0;
}
 /*******************************************END*************************************************************** */
/* Only Taida aggregation/EMS consumers use this accessor; raw protocol reads stay unchanged. */
INT16U Taida_Read_Input(INT32S legacy_address)
{
    sysPara *cfg = SysConf_GetInfo();
    if (cfg->singlePcsMaster) {
        if (legacy_address >= 2600 && legacy_address < 3800) {
            unsigned int slot = (legacy_address - 2600) / 300;
            if (!Project_Pcs_Active(cfg, slot)) return 0;
            legacy_address += 2400;
        } else if (legacy_address >= 38000 && legacy_address < 39600) {
            unsigned int slot = (legacy_address - 38000) / 200;
            if (!Project_Bms_Active(cfg, slot)) return 0;
        }
    }
    return GET_INPUT(legacy_address);
}

INT16U Tcp_Bms_Status_5MW(INT16U raw)
{
    if (BusType == S_BUS)
    {
        return (raw & 0x0001u) | ((raw & 0x0004u) >> 1);
    }
    else
    {
        return (raw & 0x0003u) | ((raw & 0x0030u) >> 2);
    }
}

INT32S Tcp_Bms_Address_5MW(INT32S address, int dual_bus)
{
    if (dual_bus) {
        /* BMS3/4地址区与BMS5/6地址区互换。 */
        if (address >= 38400 && address < 38800) {
            return address + 400;
        }
        if (address >= 38800 && address < 39200) {
            return address - 400;
        }
    } else {
        /* BMS2地址区与BMS3地址区互换。 */
        if (address >= 38200 && address < 38400) {
            return address + 200;
        }
        if (address >= 38400 && address < 38600) {
            return address - 200;
        }
    }

    return address;
}

void Init_Device_Status_5MW(void)
{
    sysPara *cfg = SysConf_GetInfo();
    INT16U pcs = 0, bms = 0;
    for (unsigned int i = 0; i < MAX_PCS_NUM; ++i)
        if (Project_Pcs_Active(cfg, i)) pcs |= 1u << i;
    for (unsigned int i = 0; i < MAX_BMS_NUM; ++i)
        if (Project_Bms_Active(cfg, i)) bms |= 1u << i;
    // SET_INPUT(STATUS_WORD1, pcs);
    SET_INPUT(STATUS_WORD11, (pcs & 1u) | ((pcs & 4u) >> 1));
    SET_INPUT(STATUS_WORD3, bms);
    // BMS_Unpack_To_Set_Hold(bms);
    SET_INPUT(NUMBER_OF_PCSs, Project_Pcs_Count(cfg));
    SET_INPUT(NUMBER_OF_BMSs, Project_Bms_Count(cfg));
}

/* ============================================================================================
 * 天合中压系统对外 Modbus TCP 通信协议 V0.16 (2026.10.19) —— Taida_PCS 点表地址表
 * --------------------------------------------------------------------------------------------
 * 类型定义见 address_map.h，本文件只存放点表数值，便于对照协议点表逐项核对。
 * 数组下标 n 从 0 开始，对应点表编号 #(n+1)。
 * ==========================================================================================*/

/* ---------------- 单台 PCS 只读输入寄存器（2600 段） ---------------- */
const PCS_TAIDA_MODULE_INPUT_ADDR_t PCS_TAIDA_MODULE_INPUT_REG[PCS_TAIDA_DEV_NUM_MAX] = {
    {   /*PCS1*/
        .Base = 2600,
        .Battery1Power                     = 2616,
        .Battery2Power                     = 2617,
        .GridActivePower                   = 2618,
        .GridReactivePower                 = 2619,
        .GridVoltageRS                     = 2620,
        .GridVoltageST                     = 2621,
        .GridVoltageTR                     = 2622,
        .Module1HourlyDiscCapacityHigh     = 2626,
        .Module1HourlyDiscCapacityLow      = 2627,
        .Module1HourlyCharCapacityHigh     = 2628,
        .Module1HourlyCharCapacityLow      = 2629,
        .Module1TotalDiscCapacityHigh      = 2642,
        .Module1TotalDiscCapacityLow       = 2643,
        .Module1TotalCharCapacityHigh      = 2644,
        .Module1TotalCharCapacityLow       = 2645,
        .Module2DailyDiscCapacityHigh      = 2666,
        .Module2DailyDiscCapacityLow       = 2667,
        .Module2DailyCharCapacityHigh      = 2668,
        .Module2DailyCharCapacityLow       = 2669,
        .Module2DailyDiscCycleTimeHigh     = 2670,
        .Module2DailyDiscCycleTimeLow      = 2671,
        .Module2DailyCharCycleTimeHigh     = 2672,
        .Module2DailyCharCycleTimeLow      = 2673,
        .Module2TotalDiscCapacityHigh      = 2674,
        .Module2TotalDiscCapacityLow       = 2675,
        .Module2TotalCharCapacityHigh      = 2676,
        .Module2TotalCharCapacityLow       = 2677,
        .Module2TotalCharCycleTimeHigh     = 2680,
        .Module2TotalCharCycleTimeLow      = 2681,
        .Module2DailyRuntimeHigh           = 2682,
        .Module2DailyRuntimeLow            = 2683,
        .Module2TotalRuntimeHigh           = 2684,
        .Module2TotalRuntimeLow            = 2685,
        .Module2StatisticalYear            = 2686,
        .Module2StatisticalMonth           = 2687,
        .Module2StatisticalDay             = 2688,
        .Module2StatisticalHour            = 2689,
        .ACHourlyDiscCapacityHigh          = 2690,
        .ACHourlyDiscCapacityLow           = 2691,
        .ACHourlyCharCapacityHigh          = 2692,
        .ACHourlyCharCapacityLow           = 2693,
        .ACHourlyDiscCycleTimeHigh         = 2694,
        .ACHourlyDiscCycleTimeLow          = 2695,
        .ACHourlyCharCycleTimeHigh         = 2696,
        .ACHourlyCharCycleTimeLow          = 2697,
        .ACDailyDiscCapacityHigh           = 2698,
        .ACDailyDiscCapacityLow            = 2699,
        .ACDailyCharCapacityHigh           = 2700,
        .ACDailyCharCapacityLow            = 2701,
        .ACDailyDiscCycleTimeHigh          = 2702,
        .ACDailyDiscCycleTimeLow           = 2703,
        .ACDailyCharCycleTimeHigh          = 2704,
        .ACDailyCharCycleTimeLow           = 2705,
        .ACTotalDiscCapacityHigh           = 2706,
        .ACTotalDiscCapacityLow            = 2707,
        .ACTotalCharCapacityHigh           = 2708,
        .ACTotalCharCapacityLow            = 2709,
        .ACTotalDiscCycleTimeHigh          = 2710,
        .ACTotalDiscCycleTimeLow           = 2711,
        .ACTotalCharCycleTimeHigh          = 2712,
        .ACTotalCharCycleTimeLow           = 2713,
        .ACHourlyCapReactiveEnergyHigh     = 2714,
        .ACHourlyCapReactiveEnergyLow      = 2715,
        .ACHourlyIndReactiveEnergyHigh     = 2716,
        .ACHourlyIndReactiveEnergyLow      = 2717,
        .ACHourlyCapReactiveCycleTimeHigh  = 2718,
        .ACHourlyCapReactiveCycleTimeLow   = 2719,
        .ACHourlyIndReactiveCycleTimeHigh  = 2720,
        .ACHourlyIndReactiveCycleTimeLow   = 2721,
        .ACDailyCapReactiveEnergyHigh      = 2722,
        .ACDailyCapReactiveEnergyLow       = 2723,
        .ACDailyIndReactiveEnergyHigh      = 2724,
        .ACDailyIndReactiveEnergyLow       = 2725,
        .ACDailyCapReactiveCycleTimeHigh   = 2726,
        .ACDailyCapReactiveCycleTimeLow    = 2727,
        .ACDailyIndReactiveCycleTimeHigh   = 2728,
        .ACDailyIndReactiveCycleTimeLow    = 2729,
        .ACTotalCapReactiveEnergyHigh      = 2730,
        .ACTotalCapReactiveEnergyLow       = 2731,
        .ACTotalIndReactiveEnergyHigh      = 2732,
        .ACTotalIndReactiveEnergyLow       = 2733,
        .SoftwareVersionWord6              = 2751,
        .SystemFaultWord2                  = 2801,
        .SystemFaultWord4                  = 2803,
        .SystemWarningWord2                = 2811,
        .ControllerFaultWord1              = 2817,
        .Module1FaultWords = {2819, 2830},
        .Module2FaultWords = {2833, 2844},
    },
    {   /*PCS2*/
        .Base = 2900,
        .Battery1Power                     = 2916,
        .Battery2Power                     = 2917,
        .GridActivePower                   = 2918,
        .GridReactivePower                 = 2919,
        .GridVoltageRS                     = 2920,
        .GridVoltageST                     = 2921,
        .GridVoltageTR                     = 2922,
        .Module1HourlyDiscCapacityHigh     = 2926,
        .Module1HourlyDiscCapacityLow      = 2927,
        .Module1HourlyCharCapacityHigh     = 2928,
        .Module1HourlyCharCapacityLow      = 2929,
        .Module1TotalDiscCapacityHigh      = 2942,
        .Module1TotalDiscCapacityLow       = 2943,
        .Module1TotalCharCapacityHigh      = 2944,
        .Module1TotalCharCapacityLow       = 2945,
        .Module2DailyDiscCapacityHigh      = 2966,
        .Module2DailyDiscCapacityLow       = 2967,
        .Module2DailyCharCapacityHigh      = 2968,
        .Module2DailyCharCapacityLow       = 2969,
        .Module2DailyDiscCycleTimeHigh     = 2970,
        .Module2DailyDiscCycleTimeLow      = 2971,
        .Module2DailyCharCycleTimeHigh     = 2972,
        .Module2DailyCharCycleTimeLow      = 2973,
        .Module2TotalDiscCapacityHigh      = 2974,
        .Module2TotalDiscCapacityLow       = 2975,
        .Module2TotalCharCapacityHigh      = 2976,
        .Module2TotalCharCapacityLow       = 2977,
        .Module2TotalCharCycleTimeHigh     = 2980,
        .Module2TotalCharCycleTimeLow      = 2981,
        .Module2DailyRuntimeHigh           = 2982,
        .Module2DailyRuntimeLow            = 2983,
        .Module2TotalRuntimeHigh           = 2984,
        .Module2TotalRuntimeLow            = 2985,
        .Module2StatisticalYear            = 2986,
        .Module2StatisticalMonth           = 2987,
        .Module2StatisticalDay             = 2988,
        .Module2StatisticalHour            = 2989,
        .ACHourlyDiscCapacityHigh          = 2990,
        .ACHourlyDiscCapacityLow           = 2991,
        .ACHourlyCharCapacityHigh          = 2992,
        .ACHourlyCharCapacityLow           = 2993,
        .ACHourlyDiscCycleTimeHigh         = 2994,
        .ACHourlyDiscCycleTimeLow          = 2995,
        .ACHourlyCharCycleTimeHigh         = 2996,
        .ACHourlyCharCycleTimeLow          = 2997,
        .ACDailyDiscCapacityHigh           = 2998,
        .ACDailyDiscCapacityLow            = 2999,
        .ACDailyCharCapacityHigh           = 3000,
        .ACDailyCharCapacityLow            = 3001,
        .ACDailyDiscCycleTimeHigh          = 3002,
        .ACDailyDiscCycleTimeLow           = 3003,
        .ACDailyCharCycleTimeHigh          = 3004,
        .ACDailyCharCycleTimeLow           = 3005,
        .ACTotalDiscCapacityHigh           = 3006,
        .ACTotalDiscCapacityLow            = 3007,
        .ACTotalCharCapacityHigh           = 3008,
        .ACTotalCharCapacityLow            = 3009,
        .ACTotalDiscCycleTimeHigh          = 3010,
        .ACTotalDiscCycleTimeLow           = 3011,
        .ACTotalCharCycleTimeHigh          = 3012,
        .ACTotalCharCycleTimeLow           = 3013,
        .ACHourlyCapReactiveEnergyHigh     = 3014,
        .ACHourlyCapReactiveEnergyLow      = 3015,
        .ACHourlyIndReactiveEnergyHigh     = 3016,
        .ACHourlyIndReactiveEnergyLow      = 3017,
        .ACHourlyCapReactiveCycleTimeHigh  = 3018,
        .ACHourlyCapReactiveCycleTimeLow   = 3019,
        .ACHourlyIndReactiveCycleTimeHigh  = 3020,
        .ACHourlyIndReactiveCycleTimeLow   = 3021,
        .ACDailyCapReactiveEnergyHigh      = 3022,
        .ACDailyCapReactiveEnergyLow       = 3023,
        .ACDailyIndReactiveEnergyHigh      = 3024,
        .ACDailyIndReactiveEnergyLow       = 3025,
        .ACDailyCapReactiveCycleTimeHigh   = 3026,
        .ACDailyCapReactiveCycleTimeLow    = 3027,
        .ACDailyIndReactiveCycleTimeHigh   = 3028,
        .ACDailyIndReactiveCycleTimeLow    = 3029,
        .ACTotalCapReactiveEnergyHigh      = 3030,
        .ACTotalCapReactiveEnergyLow       = 3031,
        .ACTotalIndReactiveEnergyHigh      = 3032,
        .ACTotalIndReactiveEnergyLow       = 3033,
        .SoftwareVersionWord6              = 3051,
        .SystemFaultWord2                  = 3101,
        .SystemFaultWord4                  = 3103,
        .SystemWarningWord2                = 3111,
        .ControllerFaultWord1              = 3117,
        .Module1FaultWords = {3119, 3130},
        .Module2FaultWords = {3133, 3144},
    },
    {   /*PCS3*/
        .Base = 3200,
        .Battery1Power                     = 3216,
        .Battery2Power                     = 3217,
        .GridActivePower                   = 3218,
        .GridReactivePower                 = 3219,
        .GridVoltageRS                     = 3220,
        .GridVoltageST                     = 3221,
        .GridVoltageTR                     = 3222,
        .Module1HourlyDiscCapacityHigh     = 3226,
        .Module1HourlyDiscCapacityLow      = 3227,
        .Module1HourlyCharCapacityHigh     = 3228,
        .Module1HourlyCharCapacityLow      = 3229,
        .Module1TotalDiscCapacityHigh      = 3242,
        .Module1TotalDiscCapacityLow       = 3243,
        .Module1TotalCharCapacityHigh      = 3244,
        .Module1TotalCharCapacityLow       = 3245,
        .Module2DailyDiscCapacityHigh      = 3266,
        .Module2DailyDiscCapacityLow       = 3267,
        .Module2DailyCharCapacityHigh      = 3268,
        .Module2DailyCharCapacityLow       = 3269,
        .Module2DailyDiscCycleTimeHigh     = 3270,
        .Module2DailyDiscCycleTimeLow      = 3271,
        .Module2DailyCharCycleTimeHigh     = 3272,
        .Module2DailyCharCycleTimeLow      = 3273,
        .Module2TotalDiscCapacityHigh      = 3274,
        .Module2TotalDiscCapacityLow       = 3275,
        .Module2TotalCharCapacityHigh      = 3276,
        .Module2TotalCharCapacityLow       = 3277,
        .Module2TotalCharCycleTimeHigh     = 3280,
        .Module2TotalCharCycleTimeLow      = 3281,
        .Module2DailyRuntimeHigh           = 3282,
        .Module2DailyRuntimeLow            = 3283,
        .Module2TotalRuntimeHigh           = 3284,
        .Module2TotalRuntimeLow            = 3285,
        .Module2StatisticalYear            = 3286,
        .Module2StatisticalMonth           = 3287,
        .Module2StatisticalDay             = 3288,
        .Module2StatisticalHour            = 3289,
        .ACHourlyDiscCapacityHigh          = 3290,
        .ACHourlyDiscCapacityLow           = 3291,
        .ACHourlyCharCapacityHigh          = 3292,
        .ACHourlyCharCapacityLow           = 3293,
        .ACHourlyDiscCycleTimeHigh         = 3294,
        .ACHourlyDiscCycleTimeLow          = 3295,
        .ACHourlyCharCycleTimeHigh         = 3296,
        .ACHourlyCharCycleTimeLow          = 3297,
        .ACDailyDiscCapacityHigh           = 3298,
        .ACDailyDiscCapacityLow            = 3299,
        .ACDailyCharCapacityHigh           = 3300,
        .ACDailyCharCapacityLow            = 3301,
        .ACDailyDiscCycleTimeHigh          = 3302,
        .ACDailyDiscCycleTimeLow           = 3303,
        .ACDailyCharCycleTimeHigh          = 3304,
        .ACDailyCharCycleTimeLow           = 3305,
        .ACTotalDiscCapacityHigh           = 3306,
        .ACTotalDiscCapacityLow            = 3307,
        .ACTotalCharCapacityHigh           = 3308,
        .ACTotalCharCapacityLow            = 3309,
        .ACTotalDiscCycleTimeHigh          = 3310,
        .ACTotalDiscCycleTimeLow           = 3311,
        .ACTotalCharCycleTimeHigh          = 3312,
        .ACTotalCharCycleTimeLow           = 3313,
        .ACHourlyCapReactiveEnergyHigh     = 3314,
        .ACHourlyCapReactiveEnergyLow      = 3315,
        .ACHourlyIndReactiveEnergyHigh     = 3316,
        .ACHourlyIndReactiveEnergyLow      = 3317,
        .ACHourlyCapReactiveCycleTimeHigh  = 3318,
        .ACHourlyCapReactiveCycleTimeLow   = 3319,
        .ACHourlyIndReactiveCycleTimeHigh  = 3320,
        .ACHourlyIndReactiveCycleTimeLow   = 3321,
        .ACDailyCapReactiveEnergyHigh      = 3322,
        .ACDailyCapReactiveEnergyLow       = 3323,
        .ACDailyIndReactiveEnergyHigh      = 3324,
        .ACDailyIndReactiveEnergyLow       = 3325,
        .ACDailyCapReactiveCycleTimeHigh   = 3326,
        .ACDailyCapReactiveCycleTimeLow    = 3327,
        .ACDailyIndReactiveCycleTimeHigh   = 3328,
        .ACDailyIndReactiveCycleTimeLow    = 3329,
        .ACTotalCapReactiveEnergyHigh      = 3330,
        .ACTotalCapReactiveEnergyLow       = 3331,
        .ACTotalIndReactiveEnergyHigh      = 3332,
        .ACTotalIndReactiveEnergyLow       = 3333,
        .SoftwareVersionWord6              = 3351,
        .SystemFaultWord2                  = 3401,
        .SystemFaultWord4                  = 3403,
        .SystemWarningWord2                = 3411,
        .ControllerFaultWord1              = 3417,
        .Module1FaultWords = {3419, 3430},
        .Module2FaultWords = {3433, 3444},
    },
    {   /*PCS4*/
        .Base = 3500,
        .Battery1Power                     = 3516,
        .Battery2Power                     = 3517,
        .GridActivePower                   = 3518,
        .GridReactivePower                 = 3519,
        .GridVoltageRS                     = 3520,
        .GridVoltageST                     = 3521,
        .GridVoltageTR                     = 3522,
        .Module1HourlyDiscCapacityHigh     = 3526,
        .Module1HourlyDiscCapacityLow      = 3527,
        .Module1HourlyCharCapacityHigh     = 3528,
        .Module1HourlyCharCapacityLow      = 3529,
        .Module1TotalDiscCapacityHigh      = 3542,
        .Module1TotalDiscCapacityLow       = 3543,
        .Module1TotalCharCapacityHigh      = 3544,
        .Module1TotalCharCapacityLow       = 3545,
        .Module2DailyDiscCapacityHigh      = 3566,
        .Module2DailyDiscCapacityLow       = 3567,
        .Module2DailyCharCapacityHigh      = 3568,
        .Module2DailyCharCapacityLow       = 3569,
        .Module2DailyDiscCycleTimeHigh     = 3570,
        .Module2DailyDiscCycleTimeLow      = 3571,
        .Module2DailyCharCycleTimeHigh     = 3572,
        .Module2DailyCharCycleTimeLow      = 3573,
        .Module2TotalDiscCapacityHigh      = 3574,
        .Module2TotalDiscCapacityLow       = 3575,
        .Module2TotalCharCapacityHigh      = 3576,
        .Module2TotalCharCapacityLow       = 3577,
        .Module2TotalCharCycleTimeHigh     = 3580,
        .Module2TotalCharCycleTimeLow      = 3581,
        .Module2DailyRuntimeHigh           = 3582,
        .Module2DailyRuntimeLow            = 3583,
        .Module2TotalRuntimeHigh           = 3584,
        .Module2TotalRuntimeLow            = 3585,
        .Module2StatisticalYear            = 3586,
        .Module2StatisticalMonth           = 3587,
        .Module2StatisticalDay             = 3588,
        .Module2StatisticalHour            = 3589,
        .ACHourlyDiscCapacityHigh          = 3590,
        .ACHourlyDiscCapacityLow           = 3591,
        .ACHourlyCharCapacityHigh          = 3592,
        .ACHourlyCharCapacityLow           = 3593,
        .ACHourlyDiscCycleTimeHigh         = 3594,
        .ACHourlyDiscCycleTimeLow          = 3595,
        .ACHourlyCharCycleTimeHigh         = 3596,
        .ACHourlyCharCycleTimeLow          = 3597,
        .ACDailyDiscCapacityHigh           = 3598,
        .ACDailyDiscCapacityLow            = 3599,
        .ACDailyCharCapacityHigh           = 3600,
        .ACDailyCharCapacityLow            = 3601,
        .ACDailyDiscCycleTimeHigh          = 3602,
        .ACDailyDiscCycleTimeLow           = 3603,
        .ACDailyCharCycleTimeHigh          = 3604,
        .ACDailyCharCycleTimeLow           = 3605,
        .ACTotalDiscCapacityHigh           = 3606,
        .ACTotalDiscCapacityLow            = 3607,
        .ACTotalCharCapacityHigh           = 3608,
        .ACTotalCharCapacityLow            = 3609,
        .ACTotalDiscCycleTimeHigh          = 3610,
        .ACTotalDiscCycleTimeLow           = 3611,
        .ACTotalCharCycleTimeHigh          = 3612,
        .ACTotalCharCycleTimeLow           = 3613,
        .ACHourlyCapReactiveEnergyHigh     = 3614,
        .ACHourlyCapReactiveEnergyLow      = 3615,
        .ACHourlyIndReactiveEnergyHigh     = 3616,
        .ACHourlyIndReactiveEnergyLow      = 3617,
        .ACHourlyCapReactiveCycleTimeHigh  = 3618,
        .ACHourlyCapReactiveCycleTimeLow   = 3619,
        .ACHourlyIndReactiveCycleTimeHigh  = 3620,
        .ACHourlyIndReactiveCycleTimeLow   = 3621,
        .ACDailyCapReactiveEnergyHigh      = 3622,
        .ACDailyCapReactiveEnergyLow       = 3623,
        .ACDailyIndReactiveEnergyHigh      = 3624,
        .ACDailyIndReactiveEnergyLow       = 3625,
        .ACDailyCapReactiveCycleTimeHigh   = 3626,
        .ACDailyCapReactiveCycleTimeLow    = 3627,
        .ACDailyIndReactiveCycleTimeHigh   = 3628,
        .ACDailyIndReactiveCycleTimeLow    = 3629,
        .ACTotalCapReactiveEnergyHigh      = 3630,
        .ACTotalCapReactiveEnergyLow       = 3631,
        .ACTotalIndReactiveEnergyHigh      = 3632,
        .ACTotalIndReactiveEnergyLow       = 3633,
        .SoftwareVersionWord6              = 3651,
        .SystemFaultWord2                  = 3701,
        .SystemFaultWord4                  = 3703,
        .SystemWarningWord2                = 3711,
        .ControllerFaultWord1              = 3717,
        .Module1FaultWords = {3719, 3730},
        .Module2FaultWords = {3733, 3744},
    },
};

/* ---------------- PCS 组(主机)只读输入寄存器（17000 段） ---------------- */
const PCS_TAIDA_GROUP_INPUT_ADDR_t PCS_TAIDA_GROUP_INPUT_REG[PCS_TAIDA_GROUP_NUM_MAX] = {
    {   /*PCS Group1*/
        .Base                 = 17000,
        .MasterPcsChargeDischargeStatus = 17032,
        .SlavePcsChargeDischargeStatus  = 17033,
        .GridTotalActivePower   = 17040,
        .GridTotalReactivePower = 17041,
        .GridPowerFactor        = 17042,
        .GridFrequency          = 17043,
        .POIActivePower         = 17050,
        .POIReactivePower       = 17051,
        .POIPowerFactor         = 17052,
        .BatteryTotalPower      = 17054,
        .BatteryTotalCurrent    = 17055,
        .SystemStateWord1       = 17060,
        .SystemStateWord2       = 17061,
        .SystemStateWord3       = 17062,
        .SystemOperationStatus  = 17063,
        .SystemEfficientMode1   = 17064,
        .SystemFaultWord1       = 17066,
        .SystemFaultWord2       = 17067,
        .SystemFaultWord3       = 17068,
        .SystemFaultWord4       = 17069,
        .SystemWarningWord2     = 17072,
        .DCChannel4Fault        = 17203,
        .DCChannel4Status       = 17211,
    },
    {   /*PCS Group2*/
        .Base                 = 17300,
        .MasterPcsChargeDischargeStatus = 17332,
        .SlavePcsChargeDischargeStatus  = 17333,
        .GridTotalActivePower   = 17340,
        .GridTotalReactivePower = 17341,
        .GridPowerFactor        = 17342,
        .GridFrequency          = 17343,
        .POIActivePower         = 17350,
        .POIReactivePower       = 17351,
        .POIPowerFactor         = 17352,
        .BatteryTotalPower      = 17354,
        .BatteryTotalCurrent    = 17355,
        .SystemStateWord1       = 17360,
        .SystemStateWord2       = 17361,
        .SystemStateWord3       = 17362,
        .SystemOperationStatus  = 17363,
        .SystemEfficientMode1   = 17364,
        .SystemFaultWord1       = 17366,
        .SystemFaultWord2       = 17367,
        .SystemFaultWord3       = 17368,
        .SystemFaultWord4       = 17369,
        .SystemWarningWord2     = 17372,
        .DCChannel4Fault        = 17503,
        .DCChannel4Status       = 17511,
    },
};

/* ---------------- 单台 PCS 保持寄存器（12000 段） ---------------- */
const PCS_TAIDA_HOLD_ADDR_t PCS_TAIDA_HOLD_REG[PCS_TAIDA_DEV_NUM_MAX] = {
    {   /*PCS1*/
        .Base               = 12000,
        .SetPCSMaster        = 12024,
        .Slave1ControlEnable = 12025,
        .PcsTimeYearSet      = 12026,
        .PcsTimeMonthSet     = 12027,
        .PcsTimeDaySet       = 12028,
        .PcsTimeHourSet      = 12029,
        .PcsTimeMinSet       = 12030,
        .PcsTimeSecSet       = 12031,
        .PcsTimeMsecSet      = 12032,
    },
    {   /*PCS2*/
        .Base               = 12300,
        .SetPCSMaster        = 12324,
        .Slave1ControlEnable = 12325,
        .PcsTimeYearSet      = 12326,
        .PcsTimeMonthSet     = 12327,
        .PcsTimeDaySet       = 12328,
        .PcsTimeHourSet      = 12329,
        .PcsTimeMinSet       = 12330,
        .PcsTimeSecSet       = 12331,
        .PcsTimeMsecSet      = 12332,
    },
    {   /*PCS3*/
        .Base               = 12600,
        .SetPCSMaster        = 12624,
        .Slave1ControlEnable = 12625,
        .PcsTimeYearSet      = 12626,
        .PcsTimeMonthSet     = 12627,
        .PcsTimeDaySet       = 12628,
        .PcsTimeHourSet      = 12629,
        .PcsTimeMinSet       = 12630,
        .PcsTimeSecSet       = 12631,
        .PcsTimeMsecSet      = 12632,
    },
    {   /*PCS4*/
        .Base               = 12900,
        .SetPCSMaster        = 12924,
        .Slave1ControlEnable = 12925,
        .PcsTimeYearSet      = 12926,
        .PcsTimeMonthSet     = 12927,
        .PcsTimeDaySet       = 12928,
        .PcsTimeHourSet      = 12929,
        .PcsTimeMinSet       = 12930,
        .PcsTimeSecSet       = 12931,
        .PcsTimeMsecSet      = 12932,
    },
};

/* ---------------- 5.5MW 一体机 PCS 保持寄存器（15000 段） ---------------- */
const PCS_TAIDA_5MW_HOLD_ADDR_t PCS_TAIDA_5MW_HOLD_REG[PCS_TAIDA_5MW_DEV_NUM_MAX] = {
    {   /*PCS1*/
        .Base               = 15000,
        .SetPCSMaster        = 15024,
        .Slave1ControlEnable = 15025,
        .PcsTimeYearSet      = 15026,
        .PcsTimeMonthSet     = 15027,
        .PcsTimeDaySet       = 15028,
        .PcsTimeHourSet      = 15029,
        .PcsTimeMinSet       = 15030,
        .PcsTimeSecSet       = 15031,
        .PcsTimeMsecSet      = 15032,
    },
    {   /*PCS2*/
        .Base               = 15600,
        .SetPCSMaster        = 15624,
        .Slave1ControlEnable = 15625,
        .PcsTimeYearSet      = 15626,
        .PcsTimeMonthSet     = 15627,
        .PcsTimeDaySet       = 15628,
        .PcsTimeHourSet      = 15629,
        .PcsTimeMinSet       = 15630,
        .PcsTimeSecSet       = 15631,
        .PcsTimeMsecSet      = 15632,
    },
};

/* ---------------- PCS 组控制点（27000 段） ---------------- */
const PCS_TAIDA_CTRL_ADDR_t PCS_TAIDA_CTRL_REG[PCS_TAIDA_GROUP_NUM_MAX] = {
    {   /*PCS Group1*/
        .Base                     = 27000,
        .GroupReset               = 27000,
        .GroupFaultClear          = 27001,
        .GroupToggle              = 27004,
        .GroupRocofCease          = 27005,
        .PQMode                   = 27016,
        .PFMode                   = 27017,
        .VoltVarMode              = 27018,
        .VoltWattMode             = 27019,
        .FreqWattMode             = 27020,
        .BlackStartMode           = 27021,
        .StatisticsClear          = 27039,
        .EnableCharge             = 27040,
        .EnableDischarge          = 27041,
        .MVG3MCBClose             = 27042,
        .MVG3MCBOpen              = 27043,
        .ResetRelayTrip           = 27044,
        .SetRelayOCPTh            = 27045,
        .MasterBat2DischargePowerCoef = 27046,
        .SlaveBat1DischargePowerCoef  = 27047,
        .MasterBat2ChargePowerCoef    = 27048,
        .SlaveBat1ChargePowerCoef     = 27049,
        .MasterBat1DischargePowerCoef = 27050,
        .MasterBat1ChargePowerCoef    = 27051,
        .ActivePowerReference         = 27052,
        .ReactivePowerReference       = 27053,
        .SystemOnOffCmd               = 27103,
        .PowerFactorReference         = 27110,
        .IslandVoltageReference       = 27111,
        .IslandFrequencyReference     = 27112,
        .DisDutyPRef                  = 27113,
        .EnDutyPRef                   = 27114,
        .HotStandbyEn                 = 27115,
    },
    {   /*PCS Group2*/
        .Base                     = 27300,
        .GroupReset               = 27300,
        .GroupFaultClear          = 27301,
        .GroupToggle              = 27304,
        .GroupRocofCease          = 27305,
        .PQMode                   = 27316,
        .PFMode                   = 27317,
        .VoltVarMode              = 27318,
        .VoltWattMode             = 27319,
        .FreqWattMode             = 27320,
        .BlackStartMode           = 27321,
        .StatisticsClear          = 27339,
        .EnableCharge             = 27340,
        .EnableDischarge          = 27341,
        .MVG3MCBClose             = 27342,
        .MVG3MCBOpen              = 27343,
        .ResetRelayTrip           = 27344,
        .SetRelayOCPTh            = 27345,
        .MasterBat2DischargePowerCoef = 27346,
        .SlaveBat1DischargePowerCoef  = 27347,
        .MasterBat2ChargePowerCoef    = 27348,
        .SlaveBat1ChargePowerCoef     = 27349,
        .MasterBat1DischargePowerCoef = 27350,
        .MasterBat1ChargePowerCoef    = 27351,
        .ActivePowerReference         = 27352,
        .ReactivePowerReference       = 27353,
        .SystemOnOffCmd               = 27403,
        .PowerFactorReference         = 27410,
        .IslandVoltageReference       = 27411,
        .IslandFrequencyReference     = 27412,
        .DisDutyPRef                  = 27413,
        .EnDutyPRef                   = 27414,
        .HotStandbyEn                 = 27415,
    },
};

/* ---------------- 电池堆(Bank/BAU)只读输入寄存器（38000 段） ---------------- */
const BMS_BAU_INPUT_ADDR_t BMS_BAU_INPUT_REG[BMS_DEV_NUM_MAX] = {
    {   /*Bank1*/
        .Base                  = 38000,
        .BankTotalVol           = 38000,
        .TotalSOC              = 38003,
        .BAU_SOH               = 38004,
        .SystemInsulation      = 38005,
        .ChgRemainCapacity     = 38006,
        .DchgRemainCapacity    = 38007,
        .SystemChgSOPDisplay   = 38008,
        .SystemDchgSOPDisplay  = 38009,
        .GridConnectCluNum     = 38034,
        .SystemTotalCluNum     = 38035,
        .DchgPower             = 38037,
        .ChgPower              = 38038,
        .ExternalFaultStatusSum= 38039,
        .LegacyWarningWord1    = 38040,
        .LegacyWarningWord2    = 38041,
        .EntryBlackStartMode   = 38060,
        .BlackStartStatus      = 38061,
        .GridConnectStatus     = 38062,
        .SystemRunMode         = 38071,
        .ChgDchgStatus         = 38072,
        .DryContactOutputStatus= 38073,
        .Heartbeat             = 38074,
    },
    {   /*Bank2*/
        .Base                  = 38200,
        .BankTotalVol           = 38200,
        .TotalSOC              = 38203,
        .BAU_SOH               = 38204,
        .SystemInsulation      = 38205,
        .ChgRemainCapacity     = 38206,
        .DchgRemainCapacity    = 38207,
        .SystemChgSOPDisplay   = 38208,
        .SystemDchgSOPDisplay  = 38209,
        .GridConnectCluNum     = 38234,
        .SystemTotalCluNum     = 38235,
        .DchgPower             = 38237,
        .ChgPower              = 38238,
        .ExternalFaultStatusSum= 38239,
        .LegacyWarningWord1    = 38240,
        .LegacyWarningWord2    = 38241,
        .EntryBlackStartMode   = 38260,
        .BlackStartStatus      = 38261,
        .GridConnectStatus     = 38262,
        .SystemRunMode         = 38271,
        .ChgDchgStatus         = 38272,
        .DryContactOutputStatus= 38273,
        .Heartbeat             = 38274,
    },
    {   /*Bank3*/
        .Base                  = 38400,
        .BankTotalVol           = 38400,
        .TotalSOC              = 38403,
        .BAU_SOH               = 38404,
        .SystemInsulation      = 38405,
        .ChgRemainCapacity     = 38406,
        .DchgRemainCapacity    = 38407,
        .SystemChgSOPDisplay   = 38408,
        .SystemDchgSOPDisplay  = 38409,
        .GridConnectCluNum     = 38434,
        .SystemTotalCluNum     = 38435,
        .DchgPower             = 38437,
        .ChgPower              = 38438,
        .ExternalFaultStatusSum= 38439,
        .LegacyWarningWord1    = 38440,
        .LegacyWarningWord2    = 38441,
        .EntryBlackStartMode   = 38460,
        .BlackStartStatus      = 38461,
        .GridConnectStatus     = 38462,
        .SystemRunMode         = 38471,
        .ChgDchgStatus         = 38472,
        .DryContactOutputStatus= 38473,
        .Heartbeat             = 38474,
    },
    {   /*Bank4*/
        .Base                  = 38600,
        .BankTotalVol           = 38600,
        .TotalSOC              = 38603,
        .BAU_SOH               = 38604,
        .SystemInsulation      = 38605,
        .ChgRemainCapacity     = 38606,
        .DchgRemainCapacity    = 38607,
        .SystemChgSOPDisplay   = 38608,
        .SystemDchgSOPDisplay  = 38609,
        .GridConnectCluNum     = 38634,
        .SystemTotalCluNum     = 38635,
        .DchgPower             = 38637,
        .ChgPower              = 38638,
        .ExternalFaultStatusSum= 38639,
        .LegacyWarningWord1    = 38640,
        .LegacyWarningWord2    = 38641,
        .EntryBlackStartMode   = 38660,
        .BlackStartStatus      = 38661,
        .GridConnectStatus     = 38662,
        .SystemRunMode         = 38671,
        .ChgDchgStatus         = 38672,
        .DryContactOutputStatus= 38673,
        .Heartbeat             = 38674,
    },
    {   /*Bank5*/
        .Base                  = 38800,
        .BankTotalVol           = 38800,
        .TotalSOC              = 38803,
        .BAU_SOH               = 38804,
        .SystemInsulation      = 38805,
        .ChgRemainCapacity     = 38806,
        .DchgRemainCapacity    = 38807,
        .SystemChgSOPDisplay   = 38808,
        .SystemDchgSOPDisplay  = 38809,
        .GridConnectCluNum     = 38834,
        .SystemTotalCluNum     = 38835,
        .DchgPower             = 38837,
        .ChgPower              = 38838,
        .ExternalFaultStatusSum= 38839,
        .LegacyWarningWord1    = 38840,
        .LegacyWarningWord2    = 38841,
        .EntryBlackStartMode   = 38860,
        .BlackStartStatus      = 38861,
        .GridConnectStatus     = 38862,
        .SystemRunMode         = 38871,
        .ChgDchgStatus         = 38872,
        .DryContactOutputStatus= 38873,
        .Heartbeat             = 38874,
    },
    {   /*Bank6*/
        .Base                  = 39000,
        .BankTotalVol           = 39000,
        .TotalSOC              = 39003,
        .BAU_SOH               = 39004,
        .SystemInsulation      = 39005,
        .ChgRemainCapacity     = 39006,
        .DchgRemainCapacity    = 39007,
        .SystemChgSOPDisplay   = 39008,
        .SystemDchgSOPDisplay  = 39009,
        .GridConnectCluNum     = 39034,
        .SystemTotalCluNum     = 39035,
        .DchgPower             = 39037,
        .ChgPower              = 39038,
        .ExternalFaultStatusSum= 39039,
        .LegacyWarningWord1    = 39040,
        .LegacyWarningWord2    = 39041,
        .EntryBlackStartMode   = 39060,
        .BlackStartStatus      = 39061,
        .GridConnectStatus     = 39062,
        .SystemRunMode         = 39071,
        .ChgDchgStatus         = 39072,
        .DryContactOutputStatus= 39073,
        .Heartbeat             = 39074,
    },
    {   /*Bank7*/
        .Base                  = 39200,
        .BankTotalVol           = 39200,
        .TotalSOC              = 39203,
        .BAU_SOH               = 39204,
        .SystemInsulation      = 39205,
        .ChgRemainCapacity     = 39206,
        .DchgRemainCapacity    = 39207,
        .SystemChgSOPDisplay   = 39208,
        .SystemDchgSOPDisplay  = 39209,
        .GridConnectCluNum     = 39234,
        .SystemTotalCluNum     = 39235,
        .DchgPower             = 39237,
        .ChgPower              = 39238,
        .ExternalFaultStatusSum= 39239,
        .LegacyWarningWord1    = 39240,
        .LegacyWarningWord2    = 39241,
        .EntryBlackStartMode   = 39260,
        .BlackStartStatus      = 39261,
        .GridConnectStatus     = 39262,
        .SystemRunMode         = 39271,
        .ChgDchgStatus         = 39272,
        .DryContactOutputStatus= 39273,
        .Heartbeat             = 39274,
    },
    {   /*Bank8*/
        .Base                  = 39400,
        .BankTotalVol           = 39400,
        .TotalSOC              = 39403,
        .BAU_SOH               = 39404,
        .SystemInsulation      = 39405,
        .ChgRemainCapacity     = 39406,
        .DchgRemainCapacity    = 39407,
        .SystemChgSOPDisplay   = 39408,
        .SystemDchgSOPDisplay  = 39409,
        .GridConnectCluNum     = 39434,
        .SystemTotalCluNum     = 39435,
        .DchgPower             = 39437,
        .ChgPower              = 39438,
        .ExternalFaultStatusSum= 39439,
        .LegacyWarningWord1    = 39440,
        .LegacyWarningWord2    = 39441,
        .EntryBlackStartMode   = 39460,
        .BlackStartStatus      = 39461,
        .GridConnectStatus     = 39462,
        .SystemRunMode         = 39471,
        .ChgDchgStatus         = 39472,
        .DryContactOutputStatus= 39473,
        .Heartbeat             = 39474,
    },
};

/* ---------------- 总系统(System)状态字 ---------------- */
const MVEMS_SYS_INPUT_ADDR_t MVEMS_SYS_INPUT_REG = {
    .SystemStatusDetailed        = 106,
    .SystemFaultSummary          = 107,
    .SystemStatusSummary         = 108,
    .ActivePower                 = 125,
    .StatusWord1_PcsComm10MW     = 218,
    .StatusWord3                 = 220,
    .StatusWord6_PcsVersionCheck = 223,
    .StatusWord7_PcsSafetyCheck  = 224,
    .StatusWord8_PcsModeCheck    = 225,
    .StatusWord11_PcsComm5MW     = 228,
};

/* ---------------- 中压(MV)测控装置 ---------------- */
const MVEMS_MV_INPUT_ADDR_t MVEMS_MV_INPUT_REG = {
    .UPSStatus         = 27109,
    .TripSummaryTotal  = 27600,
    .AlarmSummaryTotal = 27601,
    .HeavyGasTrip              = 27605,
    .LightGasAlarm             = 27606,
    .OilUltraHighTempTrip      = 27607,
    .PressureReliefTrip        = 27610,
    .UltraLowOilLevelTrip      = 27611,
    .WindingOverTempTrip       = 27612,
    .AuxTransformerOverTempTrip= 27613,
    .SmokeDetectorTrip         = 27619,
    .G02BreakerOpen            = 27631,
    .G02BreakerClosed          = 27632,
    .EmergencyStop             = 27650,
    .Protection_Summary_Signal = 25969,
};

/* ============================================================================================
 * 天合中压系统对外 Modbus TCP 通信协议 V1.00 (2026.09.14) —— PCS_TRINA 点表地址表
 *   PCS_TRINA_MODULE_INPUT_REG[n]  2600  + 300 * n
 *   PCS_TRINA_HOLD_REG[n]         12000 + 700 * n
 *   PCS_TRINA_GROUP_INPUT_REG[g]  17000 + 300 * g
 *   PCS_TRINA_CTRL_REG[n]         27000 + 300 * n
 * 注：Legacy* 成员在 V1.00 点表中未定义，沿用原设备偏移，仅为消除裸数字。
 * ==========================================================================================*/

const PCS_TRINA_MODULE_INPUT_ADDR_t PCS_TRINA_MODULE_INPUT_REG[PCS_TRINA_DEV_NUM_MAX] = {
    {   /*PCS1*/
        .Base                             = 2600,
        .MsCtrlDspFaultTable4             = 2626,
        .MsCtrlDspWarningTable1           = 2627,
        .BlackStartState                  = 2639,
        .Branch1HourlyChargingCapHigh     = 2664,
        .Branch1HourlyChargingCapLow      = 2665,
        .Branch1HourlyDischargingCapHigh  = 2666,
        .Branch1HourlyDischargingCapLow   = 2667,
        .Branch1DailyChargingCapHigh      = 2668,
        .Branch1DailyChargingCapLow       = 2669,
        .Branch1DailyDischargingCapHigh   = 2670,
        .Branch1DailyDischargingCapLow    = 2671,
        .Branch1TotalChargingCapHigh      = 2672,
        .Branch1TotalChargingCapLow       = 2673,
        .Branch1TotalDischargingCapHigh   = 2674,
        .Branch1TotalDischargingCapLow    = 2675,
        .Branch2HourlyChargingCapHigh     = 2676,
        .Branch2HourlyChargingCapLow      = 2677,
        .Branch2HourlyDischargingCapHigh  = 2678,
        .Branch2HourlyDischargingCapLow   = 2679,
        .Branch2DailyChargingCapHigh      = 2680,
        .Branch2DailyChargingCapLow       = 2681,
        .Branch2DailyDischargingCapHigh   = 2682,
        .Branch2DailyDischargingCapLow    = 2683,
        .Branch2TotalChargingCapHigh      = 2684,
        .Branch2TotalChargingCapLow       = 2685,
        .Branch2TotalDischargingCapHigh   = 2686,
        .Branch2TotalDischargingCapLow    = 2687,
        .SystemRunStatus                  = 2700,
        .GridVoltageRSPCC                 = 2703,
        .GridVoltageSTPCC                 = 2704,
        .GridVoltageTRPCC                 = 2705,
        .GridFrequencyPCC                 = 2706,
        .DCPowerTotal                     = 2710,
        .ActivePowerTotalPCC              = 2711,
        .ReactivePowerTotalPCC            = 2712,
        .ApparentPowerTotalPCC            = 2713,
        .PowerFactorTotalPCC              = 2714,
        .BatteryCurrentTotal              = 2715,
        .BatteryPowerJ1                   = 2741,
        .BatteryPowerJ2                   = 2748,
    },
    {   /*PCS2*/
        .Base                             = 2900,
        .MsCtrlDspFaultTable4             = 2926,
        .MsCtrlDspWarningTable1           = 2927,
        .BlackStartState                  = 2939,
        .Branch1HourlyChargingCapHigh     = 2964,
        .Branch1HourlyChargingCapLow      = 2965,
        .Branch1HourlyDischargingCapHigh  = 2966,
        .Branch1HourlyDischargingCapLow   = 2967,
        .Branch1DailyChargingCapHigh      = 2968,
        .Branch1DailyChargingCapLow       = 2969,
        .Branch1DailyDischargingCapHigh   = 2970,
        .Branch1DailyDischargingCapLow    = 2971,
        .Branch1TotalChargingCapHigh      = 2972,
        .Branch1TotalChargingCapLow       = 2973,
        .Branch1TotalDischargingCapHigh   = 2974,
        .Branch1TotalDischargingCapLow    = 2975,
        .Branch2HourlyChargingCapHigh     = 2976,
        .Branch2HourlyChargingCapLow      = 2977,
        .Branch2HourlyDischargingCapHigh  = 2978,
        .Branch2HourlyDischargingCapLow   = 2979,
        .Branch2DailyChargingCapHigh      = 2980,
        .Branch2DailyChargingCapLow       = 2981,
        .Branch2DailyDischargingCapHigh   = 2982,
        .Branch2DailyDischargingCapLow    = 2983,
        .Branch2TotalChargingCapHigh      = 2984,
        .Branch2TotalChargingCapLow       = 2985,
        .Branch2TotalDischargingCapHigh   = 2986,
        .Branch2TotalDischargingCapLow    = 2987,
        .SystemRunStatus                  = 3000,
        .GridVoltageRSPCC                 = 3003,
        .GridVoltageSTPCC                 = 3004,
        .GridVoltageTRPCC                 = 3005,
        .GridFrequencyPCC                 = 3006,
        .DCPowerTotal                     = 3010,
        .ActivePowerTotalPCC              = 3011,
        .ReactivePowerTotalPCC            = 3012,
        .ApparentPowerTotalPCC            = 3013,
        .PowerFactorTotalPCC              = 3014,
        .BatteryCurrentTotal              = 3015,
        .BatteryPowerJ1                   = 3041,
        .BatteryPowerJ2                   = 3048,
    },
    {   /*PCS3*/
        .Base                             = 3200,
        .MsCtrlDspFaultTable4             = 3226,
        .MsCtrlDspWarningTable1           = 3227,
        .BlackStartState                  = 3239,
        .Branch1HourlyChargingCapHigh     = 3264,
        .Branch1HourlyChargingCapLow      = 3265,
        .Branch1HourlyDischargingCapHigh  = 3266,
        .Branch1HourlyDischargingCapLow   = 3267,
        .Branch1DailyChargingCapHigh      = 3268,
        .Branch1DailyChargingCapLow       = 3269,
        .Branch1DailyDischargingCapHigh   = 3270,
        .Branch1DailyDischargingCapLow    = 3271,
        .Branch1TotalChargingCapHigh      = 3272,
        .Branch1TotalChargingCapLow       = 3273,
        .Branch1TotalDischargingCapHigh   = 3274,
        .Branch1TotalDischargingCapLow    = 3275,
        .Branch2HourlyChargingCapHigh     = 3276,
        .Branch2HourlyChargingCapLow      = 3277,
        .Branch2HourlyDischargingCapHigh  = 3278,
        .Branch2HourlyDischargingCapLow   = 3279,
        .Branch2DailyChargingCapHigh      = 3280,
        .Branch2DailyChargingCapLow       = 3281,
        .Branch2DailyDischargingCapHigh   = 3282,
        .Branch2DailyDischargingCapLow    = 3283,
        .Branch2TotalChargingCapHigh      = 3284,
        .Branch2TotalChargingCapLow       = 3285,
        .Branch2TotalDischargingCapHigh   = 3286,
        .Branch2TotalDischargingCapLow    = 3287,
        .SystemRunStatus                  = 3300,
        .GridVoltageRSPCC                 = 3303,
        .GridVoltageSTPCC                 = 3304,
        .GridVoltageTRPCC                 = 3305,
        .GridFrequencyPCC                 = 3306,
        .DCPowerTotal                     = 3310,
        .ActivePowerTotalPCC              = 3311,
        .ReactivePowerTotalPCC            = 3312,
        .ApparentPowerTotalPCC            = 3313,
        .PowerFactorTotalPCC              = 3314,
        .BatteryCurrentTotal              = 3315,
        .BatteryPowerJ1                   = 3341,
        .BatteryPowerJ2                   = 3348,
    },
    {   /*PCS4*/
        .Base                             = 3500,
        .MsCtrlDspFaultTable4             = 3526,
        .MsCtrlDspWarningTable1           = 3527,
        .BlackStartState                  = 3539,
        .Branch1HourlyChargingCapHigh     = 3564,
        .Branch1HourlyChargingCapLow      = 3565,
        .Branch1HourlyDischargingCapHigh  = 3566,
        .Branch1HourlyDischargingCapLow   = 3567,
        .Branch1DailyChargingCapHigh      = 3568,
        .Branch1DailyChargingCapLow       = 3569,
        .Branch1DailyDischargingCapHigh   = 3570,
        .Branch1DailyDischargingCapLow    = 3571,
        .Branch1TotalChargingCapHigh      = 3572,
        .Branch1TotalChargingCapLow       = 3573,
        .Branch1TotalDischargingCapHigh   = 3574,
        .Branch1TotalDischargingCapLow    = 3575,
        .Branch2HourlyChargingCapHigh     = 3576,
        .Branch2HourlyChargingCapLow      = 3577,
        .Branch2HourlyDischargingCapHigh  = 3578,
        .Branch2HourlyDischargingCapLow   = 3579,
        .Branch2DailyChargingCapHigh      = 3580,
        .Branch2DailyChargingCapLow       = 3581,
        .Branch2DailyDischargingCapHigh   = 3582,
        .Branch2DailyDischargingCapLow    = 3583,
        .Branch2TotalChargingCapHigh      = 3584,
        .Branch2TotalChargingCapLow       = 3585,
        .Branch2TotalDischargingCapHigh   = 3586,
        .Branch2TotalDischargingCapLow    = 3587,
        .SystemRunStatus                  = 3600,
        .GridVoltageRSPCC                 = 3603,
        .GridVoltageSTPCC                 = 3604,
        .GridVoltageTRPCC                 = 3605,
        .GridFrequencyPCC                 = 3606,
        .DCPowerTotal                     = 3610,
        .ActivePowerTotalPCC              = 3611,
        .ReactivePowerTotalPCC            = 3612,
        .ApparentPowerTotalPCC            = 3613,
        .PowerFactorTotalPCC              = 3614,
        .BatteryCurrentTotal              = 3615,
        .BatteryPowerJ1                   = 3641,
        .BatteryPowerJ2                   = 3648,
    },
};

const PCS_TRINA_HOLD_ADDR_t PCS_TRINA_HOLD_REG[PCS_TRINA_DEV_NUM_MAX] = {
    {   /*PCS1*/
        .Base                         = 12000,
        .ParallelMode                 = 12000,
        .PowerOnCommand               = 12001,
        .OnGridOffGridConfig          = 12002,
        .OperationMode                = 12003,
        .ActivePowerSetpoint          = 12004,
        .DischargeEnable              = 12005,
        .ChargeEnable                 = 12006,
        .FaultClear                   = 12007,
        .Reset                        = 12008,
        .MasterSlaveModeSetting       = 12009,
        .SingleBusEnable              = 12010,
        .MaxChargingVoltage           = 12011,
        .MinDischargingVoltage        = 12012,
        .MaxChargingCurrent           = 12013,
        .MaxDischargingCurrent        = 12014,
        .BlackStartEnable             = 12027,
        .ZeroPowerStandbyEnable       = 12028,
        .ActiveReactivePowerPriority  = 12029,
        .ApparentPowerLimit           = 12030,
        .DischargePowerLimit          = 12031,
        .ChargePowerLimit             = 12032,
        .MasterDischargeDistCoef      = 12033,
        .MasterChargeDistCoef         = 12034,
        .BranchDischargeDistCoef      = 12035,
        .BranchChargeDistCoef         = 12036,
        .Droop_PQ                     = 12037,
        .ReactivePowerEnable          = 12061,
        .FixedPowerFactor             = 12062,
        .FixedCosphi                  = 12063,
        .FixedReactivePowerPercent    = 12064,
        .UpgradeSignal                = 12077,
        .FactoryReset                 = 12078,
        .SystemTimeSec                = 12094,
        .SystemTimeMin                = 12095,
        .SystemTimeHour               = 12096,
        .SystemTimeDay                = 12097,
        .SystemTimeMonth              = 12098,
        .SystemTimeYear               = 12099,
        .CountryCode                  = 12100,
        .J1CurrentRange               = { 12440, 12461 },
        .J2CurrentRange               = { 12480, 12501 },
    },
    {   /*PCS2*/
        .Base                         = 12700,
        .ParallelMode                 = 12700,
        .PowerOnCommand               = 12701,
        .OnGridOffGridConfig          = 12702,
        .OperationMode                = 12703,
        .ActivePowerSetpoint          = 12704,
        .DischargeEnable              = 12705,
        .ChargeEnable                 = 12706,
        .FaultClear                   = 12707,
        .Reset                        = 12708,
        .MasterSlaveModeSetting       = 12709,
        .SingleBusEnable              = 12710,
        .MaxChargingVoltage           = 12711,
        .MinDischargingVoltage        = 12712,
        .MaxChargingCurrent           = 12713,
        .MaxDischargingCurrent        = 12714,
        .BlackStartEnable             = 12727,
        .ZeroPowerStandbyEnable       = 12728,
        .ActiveReactivePowerPriority  = 12729,
        .ApparentPowerLimit           = 12730,
        .DischargePowerLimit          = 12731,
        .ChargePowerLimit             = 12732,
        .MasterDischargeDistCoef      = 12733,
        .MasterChargeDistCoef         = 12734,
        .BranchDischargeDistCoef      = 12735,
        .BranchChargeDistCoef         = 12736,
        .Droop_PQ                     = 12737,
        .ReactivePowerEnable          = 12761,
        .FixedPowerFactor             = 12762,
        .FixedCosphi                  = 12763,
        .FixedReactivePowerPercent    = 12764,
        .UpgradeSignal                = 12777,
        .FactoryReset                 = 12778,
        .SystemTimeSec                = 12794,
        .SystemTimeMin                = 12795,
        .SystemTimeHour               = 12796,
        .SystemTimeDay                = 12797,
        .SystemTimeMonth              = 12798,
        .SystemTimeYear               = 12799,
        .CountryCode                  = 12800,
        .J1CurrentRange               = { 13140, 13161 },
        .J2CurrentRange               = { 13180, 13201 },
    },
    {   /*PCS3*/
        .Base                         = 13400,
        .ParallelMode                 = 13400,
        .PowerOnCommand               = 13401,
        .OnGridOffGridConfig          = 13402,
        .OperationMode                = 13403,
        .ActivePowerSetpoint          = 13404,
        .DischargeEnable              = 13405,
        .ChargeEnable                 = 13406,
        .FaultClear                   = 13407,
        .Reset                        = 13408,
        .MasterSlaveModeSetting       = 13409,
        .SingleBusEnable              = 13410,
        .MaxChargingVoltage           = 13411,
        .MinDischargingVoltage        = 13412,
        .MaxChargingCurrent           = 13413,
        .MaxDischargingCurrent        = 13414,
        .BlackStartEnable             = 13427,
        .ZeroPowerStandbyEnable       = 13428,
        .ActiveReactivePowerPriority  = 13429,
        .ApparentPowerLimit           = 13430,
        .DischargePowerLimit          = 13431,
        .ChargePowerLimit             = 13432,
        .MasterDischargeDistCoef      = 13433,
        .MasterChargeDistCoef         = 13434,
        .BranchDischargeDistCoef      = 13435,
        .BranchChargeDistCoef         = 13436,
        .Droop_PQ                     = 13437,
        .ReactivePowerEnable          = 13461,
        .FixedPowerFactor             = 13462,
        .FixedCosphi                  = 13463,
        .FixedReactivePowerPercent    = 13464,
        .UpgradeSignal                = 13477,
        .FactoryReset                 = 13478,
        .SystemTimeSec                = 13494,
        .SystemTimeMin                = 13495,
        .SystemTimeHour               = 13496,
        .SystemTimeDay                = 13497,
        .SystemTimeMonth              = 13498,
        .SystemTimeYear               = 13499,
        .CountryCode                  = 13500,
        .J1CurrentRange               = { 13840, 13861 },
        .J2CurrentRange               = { 13880, 13901 },
    },
    {   /*PCS4*/
        .Base                         = 14100,
        .ParallelMode                 = 14100,
        .PowerOnCommand               = 14101,
        .OnGridOffGridConfig          = 14102,
        .OperationMode                = 14103,
        .ActivePowerSetpoint          = 14104,
        .DischargeEnable              = 14105,
        .ChargeEnable                 = 14106,
        .FaultClear                   = 14107,
        .Reset                        = 14108,
        .MasterSlaveModeSetting       = 14109,
        .SingleBusEnable              = 14110,
        .MaxChargingVoltage           = 14111,
        .MinDischargingVoltage        = 14112,
        .MaxChargingCurrent           = 14113,
        .MaxDischargingCurrent        = 14114,
        .BlackStartEnable             = 14127,
        .ZeroPowerStandbyEnable       = 14128,
        .ActiveReactivePowerPriority  = 14129,
        .ApparentPowerLimit           = 14130,
        .DischargePowerLimit          = 14131,
        .ChargePowerLimit             = 14132,
        .MasterDischargeDistCoef      = 14133,
        .MasterChargeDistCoef         = 14134,
        .BranchDischargeDistCoef      = 14135,
        .BranchChargeDistCoef         = 14136,
        .Droop_PQ                     = 14137,
        .ReactivePowerEnable          = 14161,
        .FixedPowerFactor             = 14162,
        .FixedCosphi                  = 14163,
        .FixedReactivePowerPercent    = 14164,
        .UpgradeSignal                = 14177,
        .FactoryReset                 = 14178,
        .SystemTimeSec                = 14194,
        .SystemTimeMin                = 14195,
        .SystemTimeHour               = 14196,
        .SystemTimeDay                = 14197,
        .SystemTimeMonth              = 14198,
        .SystemTimeYear               = 14199,
        .CountryCode                  = 14200,
        .J1CurrentRange               = { 14540, 14561 },
        .J2CurrentRange               = { 14580, 14601 },
    },
};

const PCS_TRINA_GROUP_INPUT_ADDR_t PCS_TRINA_GROUP_INPUT_REG[PCS_TRINA_GROUP_NUM_MAX] = {
    {   /*Group1*/
        .Base                         = 17000,
        .DCChannelFault               = { 17200, 17203 },
        .DCChannelAlarm               = { 17204, 17207 },
        .DCChannelStatus              = { 17208, 17211 },
        .DCChannelIslandFreq          = { 17212, 17215 },
        .DCChannelGridVolt            = { 17216, 17227 },
        .DCChannelActivePower         = { 17228, 17231 },
        .DCChannelBlackStartRef       = { 17236, 17239 },
        .LegacyMasterChgDchgStatus    = 17032,
        .LegacySlaveChgDchgStatus     = 17033,
        .LegacyGridPowerFactor        = 17042,
        .LegacyGridFrequency          = 17043,
        .LegacySystemStateWord1       = 17060,
        .LegacySystemOperationStatus  = 17063,
    },
    {   /*Group2*/
        .Base                         = 17300,
        .DCChannelFault               = { 17500, 17503 },
        .DCChannelAlarm               = { 17504, 17507 },
        .DCChannelStatus              = { 17508, 17511 },
        .DCChannelIslandFreq          = { 17512, 17515 },
        .DCChannelGridVolt            = { 17516, 17527 },
        .DCChannelActivePower         = { 17528, 17531 },
        .DCChannelBlackStartRef       = { 17536, 17539 },
        .LegacyMasterChgDchgStatus    = 17332,
        .LegacySlaveChgDchgStatus     = 17333,
        .LegacyGridPowerFactor        = 17342,
        .LegacyGridFrequency          = 17343,
        .LegacySystemStateWord1       = 17360,
        .LegacySystemOperationStatus  = 17363,
    },
};
const PCS_TRINA_GROUP_HOLD_ADDR_t PCS_TRINA_GROUP_HOLD_REG[PCS_TRINA_GROUP_NUM_MAX] = {
    {   /*Group1*/
        .Base                         = 27000,
        .Droop_pq               = { 27212, 27215 },

    },
    {   /*Group2*/
        .Base                         = 27300,
        .Droop_pq               = { 27512, 27515 },

    },
};

const PCS_TRINA_CTRL_ADDR_t PCS_TRINA_CTRL_REG[PCS_TRINA_DEV_NUM_MAX] = {
    {   /*PCS1*/
        .Base                         = 27000,
        .DCChannel1OnOffCmd           = 27200,
        .DCChannel2OnOffCmd           = 27201,
        .DCChannel3OnOffCmd           = 27202,
        .DCChannel4OnOffCmd           = 27203,
        .DCChannel1ActivePowerRef     = 27204,
        .DCChannel2ActivePowerRef     = 27205,
        .DCChannel3ActivePowerRef     = 27206,
        .DCChannel4ActivePowerRef     = 27207,
        .DCChannel1ReactivePowerRef   = 27208,
        .DCChannel2ReactivePowerRef   = 27209,
        .DCChannel3ReactivePowerRef   = 27210,
        .DCChannel4ReactivePowerRef   = 27211,
    },
    {   /*PCS2*/
        .Base                         = 27300,
        .DCChannel1OnOffCmd           = 27500,
        .DCChannel2OnOffCmd           = 27501,
        .DCChannel3OnOffCmd           = 27502,
        .DCChannel4OnOffCmd           = 27503,
        .DCChannel1ActivePowerRef     = 27504,
        .DCChannel2ActivePowerRef     = 27505,
        .DCChannel3ActivePowerRef     = 27506,
        .DCChannel4ActivePowerRef     = 27507,
        .DCChannel1ReactivePowerRef   = 27508,
        .DCChannel2ReactivePowerRef   = 27509,
        .DCChannel3ReactivePowerRef   = 27510,
        .DCChannel4ReactivePowerRef   = 27511,
    },
    {   /*PCS3*/
        .Base                         = 27600,
        .DCChannel1OnOffCmd           = 27800,
        .DCChannel2OnOffCmd           = 27801,
        .DCChannel3OnOffCmd           = 27802,
        .DCChannel4OnOffCmd           = 27803,
        .DCChannel1ActivePowerRef     = 27804,
        .DCChannel2ActivePowerRef     = 27805,
        .DCChannel3ActivePowerRef     = 27806,
        .DCChannel4ActivePowerRef     = 27807,
        .DCChannel1ReactivePowerRef   = 27808,
        .DCChannel2ReactivePowerRef   = 27809,
        .DCChannel3ReactivePowerRef   = 27810,
        .DCChannel4ReactivePowerRef   = 27811,
    },
    {   /*PCS4*/
        .Base                         = 27900,
        .DCChannel1OnOffCmd           = 28100,
        .DCChannel2OnOffCmd           = 28101,
        .DCChannel3OnOffCmd           = 28102,
        .DCChannel4OnOffCmd           = 28103,
        .DCChannel1ActivePowerRef     = 28104,
        .DCChannel2ActivePowerRef     = 28105,
        .DCChannel3ActivePowerRef     = 28106,
        .DCChannel4ActivePowerRef     = 28107,
        .DCChannel1ReactivePowerRef   = 28108,
        .DCChannel2ReactivePowerRef   = 28109,
        .DCChannel3ReactivePowerRef   = 28110,
        .DCChannel4ReactivePowerRef   = 28111,
    },
};
