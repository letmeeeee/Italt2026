/* ------------------------------------------------------------
 * Copyright (C) 2022
 * File Name : address_map.h
 * Project :
 * Description : 
 * File Created :
 * Author : wujinjin
 * ------------------------------------------------------------
 * Last Modified :
 * Modified By :
 * ------------------------------------------------------------
 * HISTORY :
 */

 #ifndef ADDRESS_MAP_H
 #define ADDRESS_MAP_H
#include "system.h"
 
INT16U Taida_Read_Input(INT32S legacy_address);
INT16U Tcp_Bms_Status_5MW(INT16U raw);
INT32S Tcp_Bms_Address_5MW(INT32S address, int dual_bus);
void Init_Device_Status_5MW(void);
 
 #define HOLD_SIZE 65535
 #define INPUT_SIZE 65535
 #define ALL_SIZE (INPUT_SIZE + HOLD_SIZE)
 
 
 // 联合体结构定义寄存器模块
 typedef union RegistersModel {   
     struct {
         unsigned short Input[INPUT_SIZE];
         unsigned short Hold[HOLD_SIZE];
     };
     unsigned short Reg[ALL_SIZE];
     unsigned char ubyte[ALL_SIZE*2];
 } RegistersModel,*pRegistersModel;
 
 extern volatile RegistersModel RegM;
 int Find_BANK_N(int addr, int *base_out);
 int Trina_HOLD_ADDRESS(int pcs_index, int index);
 int Find_trina_N(int addr, int *base_out);
 INT32S Taida_Addr_From_Index(INT8U pcs_index, INT32S index);
int CEM9000_INPUT(INT16U address, INT16U value);
int CEM9000_FINLAND_INPUT(INT16U address, INT16U value);
int CEM9000_UK_INPUT(INT16U address, INT16U value);
int CEM9000_SWEDEN_INPUT(INT16U address, INT16U value);
int write_bits_by_addr(INT16U addr, INT16U value,INT16U pcs_num);
int CEM9000_HOLD(INT16S value);
 INT32S G2pro_ADDR_FROM_INDEX(INT8U bms_index, INT32S index);
 INT32S Trina_Addr_From_Index(INT8U pcs_index, INT32S index);
 // 反向映射：index → address
 #define HOLD_ADDRESS(index) 


 // 正向映射：address → index
 #define HOLD_INDEX(address)


    //  // 宏定义获取映射值（同时检查有效索引）   
#define GET_HOLD(address) \
    ({ \
        (address >= 0 && address < 65535) ? \
        RegM.Hold[address] : 0; \
    })

#define SET_HOLD(address, val) \
    ({ \
        if (address >= 0 && address < 65535) \
        RegM.Hold[address] = (val); \
    })
 // 正向映射：address → index
 #define INPUT_INDEX(address) (address)

 // 反向映射：index → address
 #define INPUT_ADDRESS(index)  (index)

 
 #define BANK_SIZE 10
 #define RACK_SIZE 15
 #define R_INPUT_SIZE 1152
 #define R_ALL_SIZE (BANK_SIZE * RACK_SIZE * R_INPUT_SIZE)
 
 typedef union RankModel {
     struct {
    unsigned short Input[BANK_SIZE][RACK_SIZE * R_INPUT_SIZE];
     };
     unsigned short Reg[R_ALL_SIZE];
     unsigned char ubyte[R_ALL_SIZE * 2];
 } RankModel, *pRankModel;
 
 extern volatile RankModel RankM;
 extern INT8U BusType;


/**********************************************PE寄存器地址映射 **********************************************************************/  
#define PE_INPUT_INDEX(address) ( \
    ((address) == 4326) ? 125 : /* 有功功率低压 */ \
    ((address) == 4327) ? 127 : /* 无功功率低压 */ \
    ((address) == 4329) ? 122 : /* 低压功率因数值 */ \
    ((address) == 6513) ? 2630 : /* 模块1输出有功功率 */ \
    ((address) == 6532) ? 2893 : /* 模块1湿度 */ \
    ((address) == 6548) ? 2930 : /* 模块2输出有功功率 */ \
    ((address) == 6567) ? 3193 : /* 模块2湿度 */ \
    ((address) == 6583) ? 3230 : /* 模块3输出有功功率 */ \
    ((address) == 6611) ? 3493 : /* 模块3湿度 */ \
    ((address) == 6618) ? 3530 : /* 模块4输出有功功率 */ \
    ((address) == 6637) ? 3793 : /* 模块4湿度 */ \
    ((address) == 572) ? 153 : /* 总运行时间-年 */ \
    ((address) == 573) ? 155 : /* 总运行时间-日 */ \
    ((address) == 4300) ? 25822 : /* 中压RS相电压 */ \
    ((address) == 4301) ? 25824 : /* 中压ST相电压 */ \
    ((address) == 4302) ? 25826 : /* 中压TR相电压 */ \
    ((address) == 4303) ? 25800 : /* 中压相电流1 */ \
    ((address) == 4304) ? 25802 : /* 中压相电流2 */ \
    ((address) == 4305) ? 25804 : /* 中压相电流3 */ \
    ((address) == 4306) ? 25808 : /* 中压有功 */ \
    ((address) == 4307) ? 25810 : /* 中压无功 */ \
    ((address) == 4308) ? 25812 : /* 中压视在功率 */ \
    ((address) == 4309) ? 25814 : /* 中压功率因数 */ \
    (((address) >= 574) && ((address) < 576)) ? ((address) - 574 + 156) : \
    (((address) >= 1512) && ((address) < 1544)) ? ((address) - 1512 + 260) : \
    (((address) >= 4320) && ((address) < 4326)) ? ((address) - 4320 + 204) : \
    (((address) >= 4331) && ((address) < 4333)) ? ((address) - 4331 + 129) : \
    -1 \
)
//PE的PCS设置地址，实际设备地址映射对外地址
  #define PE_HOLD_INDEX(address) \
  (address) == 281 ? 7: /*母线功率阀值*/\
  (address) == 38 ? 6: /*PCS起停*/\
  (address) == 41 ? 1490 : /*故障复位*/\
  (address) == 262 ? 27000 : /*舱1下功率*/\
  (address) == 263 ? 27000+300:  /*舱2下功率*/\
  (address) == 5 ? 1505: /*系统对时秒～秒*/\
  (address) == 6 ? 1504: /*系统对时秒～分*/\
  (address) == 7 ? 1503 : /*系统对时秒～时*/\
  (address) == 8 ? 1502 : /*系统对时秒～日*/\
  (address) == 9 ? 1501:  /*系统对时秒～月*/\
  (address) == 10 ? 1500: -1 /*系统对时秒～年*/\


 // 反向映射：index → address，对外地址映射实际设备地址
  #define PE_HOLD_ADDRESS(index) \
  (index) == 1505 ? 5: /*系统对时秒～秒*/\
  (index) == 1504 ? 6: /*系统对时秒～分*/\
  (index) == 1503 ? 7 : /*系统对时秒～时*/\
  (index) == 1502 ? 8 : /*系统对时秒～日*/\
  (index) == 1501 ? 9:  /*系统对时秒～月*/\
  (index) == 1500 ? 10:  /*系统对时秒～年*/\
  (index) == 7 ? 281 : \
  (index) == 6 ? 38 : \
  (index) == 1490 ? 41 :  \
  (index) == 27000 ? 262 : /*舱1下功率*/\
  (index) == 27000+300 ?263 :-1 /*舱2下功率*/\
  /**********************************************END **********************************************************************/  


/**********************************************天合寄存器地址映射 **********************************************************************/  

#define Trina_INPUT_INDEX(pcs_index, address) ( \
  ((address) >= 4000 && (address) < 4007 ? ((address) - 4000 + 2600+0+300*(pcs_index-1)) : /*硬件版本～MS_FPGA变更版本*/\
  ((address) >= 4500 && (address) < 4502 ? ((address) - 4500 + 2600+8+300*(pcs_index-1)) : /*J1版本～J2版本*/\
  ((address) == 4040) ? 2600+39+300*(pcs_index-1) : /* 黑启动状态 */ \
  ((address) >= 4023 && (address) < 4039 ? ((address) - 4023 + 2600+23+300*(pcs_index-1)) : /*DSP故障信息表1～FPGA告警信息表4*/\
  ((address) >= 4050 && (address) < 4063 ? ((address) - 4050 + 2600+50+300*(pcs_index-1)) : /*ARM与控制DSP间的通讯故障1～ARM系统级故障*/\
  ((address) >= 4064 && (address) < 4088 ? ((address) - 4064 + 2600+64+300*(pcs_index-1)) : /*累积充放电量*/\
  ((address) >= 4100 && (address) < 4149 ? ((address) - 4100 + 2600+100+300*(pcs_index-1)) : /*运行状态～电池功率*/\
  ((address) >= 4502 && (address) < 4528 ? ((address) - 4502 + 2600+152+300*(pcs_index-1)) : /*DSP故障信息1～IMD软件版本号*/\
  ((address) >= 4550 && (address) < 4570 ? ((address) - 4550 + 2600+200+300*(pcs_index-1)) : /*电感电流R～ISO2Rn*/\
  ((address) >= 4580 && (address) < 4626 ? ((address) - 4580 + 2600+230+300*(pcs_index-1)) : -1 )))))))))/*R相风扇速度～ T相IGBT12温度（运行信息）*/\
)


#define Trina_HOLD_INDEX(pcs_index, address) ( \
  (address) == 3750 ? 12000+37+700*(pcs_index-1) : /*下垂模式下切换到PQ模式*/\
  ((address) >= 3000 && (address) < 3037 ? ((address) - 3000 + 12000+0+700*(pcs_index-1)) : /*并联模式～支路充电分配系数*/\
  ((address) >= 3077 && (address) < 3079 ? ((address) - 3077 + 12000+77+700*(pcs_index-1)) : /*升级信号～恢复出厂设置*/\
  ((address) >= 3086 && (address) < 3090 ? ((address) - 3086 + 12000+86+700*(pcs_index-1)) : /*IP地址*/\
  ((address) >= 3094 && (address) < 3100 ? ((address) - 3094 + 12000+94+700*(pcs_index-1)) : /*时间设置*/\
  ((address) >= 3100 && (address) < 3250 ? ((address) - 3100 + 12000+100+700*(pcs_index-1)) : /*庵桂参数*/\
  ((address) >= 3250 && (address) < 3350 ? ((address) - 3250 + 12000+250+700*(pcs_index-1)) : /*庵桂参数*/\
  ((address) >= 3350 && (address) < 3411 ? ((address) - 3350 + 12000+350+700*(pcs_index-1)) : /**/\
  ((address) >= 3800 && (address) < 3822 ? ((address) - 3800 + 12000+440+700*(pcs_index-1)) : /*桥臂电流ak~PCC电流T相B*/\
  ((address) >= 3900 && (address) < 3972 ? ((address) - 3900 + 12000+534+700*(pcs_index-1)) :   -1 )))))))))/*交流电感瞬时数值过流～ BUS2绝缘阻抗故障时间*/\
)



/***********************************************END************************************************************************************/


/**********************************************cem9000 **********************************************************************/  

#define Cem9000_INPUT_INDEX(address) ( \
  ((address) >= 80 && (address) < 88 ? ((address) - 80 + 25030) : /*变压器数据*/ \
  ((address) >= 100 && (address) < 114 ? ((address) - 100 + 25306) : /*AB线电压～N线电流*/\
  ((address) >= 114 && (address) < 116 ? ((address) - 114 + 25326) : /*总有功*/\
  ((address) >= 116 && (address) < 118 ? ((address) - 116 + 25334) : /*总无功*/\
  ((address) >= 118 && (address) < 120 ? ((address) - 118 + 25342) : /*总视在*/\
  ((address) >= 120 && (address) < 124 ? ((address) - 120 + 25350) : /*总功率因数～HZ*/\
  ((address) >= 124 && (address) < 128 ? ((address) - 124 + 25362) : /*正向有功电能～反向有功*/\
  ((address) >= 128 && (address) < 132 ? ((address) - 128 + 25368) : /*正向无功电能～反向无功*/\
  ((address) >= 0 && (address) < 14 ? ((address) - 0 + 25850) : /*高压测量IA～高压保护3I0*/\
  ((address) >= 14 && (address) < 80 ? ((address) - 14 + 27750) : /*系统频率F～CEM9000-4-20mA备用*/\
  ((address) >= 88 && (address) < 92 ? ((address) - 88 + 27816) : /*原变压器数据移入测控数据*/ \
  ((address) >= 92 && (address) < 100 ? ((address) - 92 + 25042) : /*母线桥温湿度*/ \
  -1 )))))))))))))

#define Cem9000_hold_INDEX(address) ( \
  ((address) >= 2000 && (address) < 2007 ? ((address) - 2000 + 33200) : \
  -1 )) 


/* UK 10MW YC -> MV-EMS V0.24. Both words of each big-endian float are
 * copied unchanged. Source spares and LV measuring currents without
 * matching public points (132..143) are skipped. */
#define Cem9000_UK_INPUT_INDEX(address) ( \
  (((address) >= 6) && ((address) < 14)) ? ((address) - 6 + 25856) : /* HV protection currents */ \
  (((address) >= 14) && ((address) < 76)) ? ((address) - 14 + 27750) : /* frequency, LV side 1/2 */ \
  (((address) >= 80) && ((address) < 86)) ? ((address) - 80 + 25030) : /* HV temperature/humidity, oil temperature */ \
  (((address) >= 88) && ((address) < 92)) ? ((address) - 88 + 27816) : /* LV room temperature/humidity */ \
  (((address) >= 100) && ((address) < 114)) ? ((address) - 100 + 25306) : /* PM line voltages/currents */ \
  (((address) >= 114) && ((address) < 116)) ? ((address) - 114 + 25326) : /* active power */ \
  (((address) >= 116) && ((address) < 118)) ? ((address) - 116 + 25334) : /* reactive power */ \
  (((address) >= 118) && ((address) < 120)) ? ((address) - 118 + 25342) : /* apparent power */ \
  (((address) >= 120) && ((address) < 124)) ? ((address) - 120 + 25350) : /* power factor/frequency */ \
  (((address) >= 124) && ((address) < 128)) ? ((address) - 124 + 25362) : /* active energy */ \
  (((address) >= 128) && ((address) < 132)) ? ((address) - 128 + 25368) : /* reactive energy */ \
  (((address) >= 144) && ((address) < 152)) ? ((address) - 144 + 25050) : /* HV channels 2/3 */ \
  (((address) >= 152) && ((address) < 192)) ? ((address) - 152 + 25864) : /* iRelay 50-P */ \
  (((address) >= 192) && ((address) < 204)) ? ((address) - 192 + 27820) : /* heat exchanger */ \
  -1 \
)

/* UK YX: point = (register - 1000) * 16 + bit, LSB first.
 * Only points defined in both tables are forwarded. In particular,
 * source YX62 is spare even though V0.24 has DI58 at 27662. */
#define Cem9000_UK_YX_INDEX(point) ( \
  (((point) >= 0) && ((point) < 12)) ? ((point) + 27600) : \
  (((point) >= 19) && ((point) < 27)) ? ((point) + 27600) : \
  (((point) >= 31) && ((point) < 51)) ? ((point) + 27600) : \
  ((point) == 55) ? 27655 : \
  ((point) == 61) ? 27661 : \
  ((point) == 63) ? 27663 : \
  (((point) >= 68) && ((point) < 114)) ? ((point) + 27600) : \
  (((point) >= 126) && ((point) <= 302) && (((point) - 126) % 16 == 0)) ? (27718 + ((point) - 126) / 16) : /* heat exchanger bit14 */ \
  (((point) >= 318) && ((point) < 320)) ? ((point) - 318 + 25920) : /* IN1/IN2 */ \
  ((point) == 334) ? 25922 : /* OUT1 */ \
  (((point) >= 350) && ((point) < 374)) ? ((point) - 350 + 25923) : /* faults and held protection */ \
  (((point) >= 382) && ((point) < 385)) ? ((point) - 382 + 25947) : \
  (((point) >= 390) && ((point) < 406)) ? ((point) - 390 + 25950) : /* real-time protection */ \
  (((point) >= 414) && ((point) < 417)) ? ((point) - 414 + 25966) : \
  ((point) == 430) ? 25969 : /* protection summary */ \
  ((point) == 446) ? 25970 : /* alarm summary */ \
  -1 \
)

/* Finland YC source table: 97 float32 values / 194 registers. Map only
 * named points defined by MV-EMS V0.27; source spares and unmatched values
 * return -1. Every mapped float retains its two-register order. */
#define Cem9000_Finland_Korkeamaa_INPUT_INDEX(address) ( \
  (((address) >= 14) && ((address) < 30)) ? ((address) - 14 + 27750) : /* frequency, HV/LV voltages */ \
  (((address) >= 46) && ((address) < 60)) ? ((address) - 46 + 27782) : /* LV side 2 voltages */ \
  (((address) >= 80) && ((address) < 84)) ? ((address) - 80 + 25030) : /* G01 temperature/humidity */ \
  (((address) >= 84) && ((address) < 92)) ? ((address) - 84 + 25050) : /* G02/G03 temperature/humidity */ \
  (((address) >= 92) && ((address) < 94)) ? ((address) - 92 + 25034) : /* oil surface temperature */ \
  (((address) >= 94) && ((address) < 98)) ? ((address) - 94 + 27816) : /* LV room temperature/humidity */ \
  (((address) >= 104) && ((address) < 112)) ? ((address) - 104 + 25372) : /* HV PM currents, mA -> A in driver */ \
  (((address) >= 118) && ((address) < 122)) ? ((address) - 118 + 25380) : /* HV PM power factor/frequency */ \
  (((address) >= 130) && ((address) < 144)) ? ((address) - 130 + 25306) : /* LV room 1 PM1 voltages/currents */ \
  (((address) >= 144) && ((address) < 146)) ? ((address) - 144 + 25326) : /* total active power */ \
  (((address) >= 146) && ((address) < 148)) ? ((address) - 146 + 25334) : /* total reactive power */ \
  (((address) >= 148) && ((address) < 150)) ? ((address) - 148 + 25342) : /* total apparent power */ \
  (((address) >= 150) && ((address) < 154)) ? ((address) - 150 + 25350) : /* power factor/frequency */ \
  (((address) >= 154) && ((address) < 158)) ? ((address) - 154 + 25362) : /* forward/reverse active energy */ \
  (((address) >= 158) && ((address) < 162)) ? ((address) - 158 + 25368) : /* forward/reverse reactive energy */ \
  (((address) >= 162) && ((address) < 170)) ? ((address) - 162 + 25856) : /* AR51 protection phase/residual currents */ \
  (((address) >= 170) && ((address) < 176)) ? ((address) - 170 + 25850) : /* AR51 measuring phase currents */ \
  (((address) >= 176) && ((address) < 184)) ? ((address) - 176 + 27820) : /* Air Conditioner KT */ \
  -1 \
)

#define Cem9000_Finland_Kannisto_INPUT_INDEX(address) ( \
  (((address) >= 14) && ((address) < 30)) ? ((address) - 14 + 27750) : /* frequency, HV/LV voltages */ \
  (((address) >= 46) && ((address) < 60)) ? ((address) - 46 + 27782) : /* LV side 2 voltages */ \
  (((address) >= 80) && ((address) < 84)) ? ((address) - 80 + 25030) : /* G01 temperature/humidity */ \
  (((address) >= 84) && ((address) < 92)) ? ((address) - 84 + 25050) : /* G02/G03 temperature/humidity */ \
  (((address) >= 92) && ((address) < 94)) ? ((address) - 92 + 25034) : /* oil surface temperature */ \
  (((address) >= 94) && ((address) < 98)) ? ((address) - 94 + 27816) : /* LV room temperature/humidity */ \
  (((address) >= 104) && ((address) < 112)) ? ((address) - 104 + 25372) : /* HV PM currents, mA -> A in driver */ \
  (((address) >= 118) && ((address) < 122)) ? ((address) - 118 + 25380) : /* HV PM power factor/frequency */ \
  (((address) >= 130) && ((address) < 144)) ? ((address) - 130 + 25306) : /* LV room 1 PM1 voltages/currents */ \
  (((address) >= 144) && ((address) < 146)) ? ((address) - 144 + 25326) : /* total active power */ \
  (((address) >= 146) && ((address) < 148)) ? ((address) - 146 + 25334) : /* total reactive power */ \
  (((address) >= 148) && ((address) < 150)) ? ((address) - 148 + 25342) : /* total apparent power */ \
  (((address) >= 150) && ((address) < 154)) ? ((address) - 150 + 25350) : /* power factor/frequency */ \
  (((address) >= 154) && ((address) < 158)) ? ((address) - 154 + 25362) : /* forward/reverse active energy */ \
  (((address) >= 158) && ((address) < 162)) ? ((address) - 158 + 25368) : /* forward/reverse reactive energy */ \
  (((address) >= 162) && ((address) < 170)) ? ((address) - 162 + 25856) : /* AR51 protection phase/residual currents */ \
  (((address) >= 170) && ((address) < 176)) ? ((address) - 170 + 25850) : /* AR51 measuring phase currents */ \
  (((address) >= 176) && ((address) < 178)) ? ((address) - 176 + 27836) : /* Air Conditioner KT */ \
  (((address) >= 178) && ((address) < 194)) ? ((address) - 178 + 27820) : /* Air Conditioner KT */ \
  -1 \
)



/* Finland YX point = (source register - 1000) * 16 + bit, bit 0 = LSB.
 * The source has 140 points. Reserved points have no public-register write.
 * PCS4 is 27626: the user confirmed V0.27's duplicate 27625 is a typo. */
#define Cem9000_Finland_YX_INDEX(point) ( \
  (((point) >= 0) && ((point) < 12)) ? ((point) + 27600) : \
  (((point) >= 13) && ((point) < 15)) ? ((point) + 27600) : \
  (((point) >= 19) && ((point) < 28)) ? ((point) + 27600) : \
  (((point) >= 29) && ((point) < 51)) ? ((point) + 27600) : \
  ((point) == 52) ? 27652 : \
  (((point) >= 55) && ((point) < 60)) ? ((point) + 27600) : \
  (((point) >= 68) && ((point) < 80)) ? ((point) + 27600) : \
  (((point) >= 88) && ((point) < 90)) ? ((point) + 27600) : \
  (((point) >= 103) && ((point) < 105)) ? ((point) + 27600) : \
  (((point) >= 110) && ((point) < 114)) ? ((point) + 27600) : \
  (((point) >= 114) && ((point) < 137)) ? ((point) - 114 + 25972) : /* AR51 digital inputs */ \
  (((point) >= 137) && ((point) < 140)) ? ((point) - 137 + 25969) : /* AR51 trip/alarm/control-loop fault */ \
  -1 \
)


/* 瑞典测控遥测 -> MV-EMS V0.18；float两个寄存器保持原高低字顺序。 */
#define Cem9000_Sweden_INPUT_INDEX(address) ( \
  (((address) >= 0) && ((address) < 14)) ? ((address) + 25850) : /* 高压测量/保护电流 */ \
  (((address) >= 14) && ((address) < 80)) ? ((address) - 14 + 27750) : /* 频率、低压1/2侧、PT100/4-20mA */ \
  (((address) >= 80) && ((address) < 88)) ? ((address) - 80 + 25030) : /* 高压温湿度、油面/绕组温度 */ \
  (((address) >= 88) && ((address) < 92)) ? ((address) - 88 + 27816) : /* 低压室温湿度 */ \
  (((address) >= 92) && ((address) < 100)) ? ((address) - 92 + 25042) : /* 母线桥1/2温湿度 */ \
  (((address) >= 100) && ((address) < 114)) ? ((address) - 100 + 25306) : /* 电表线电压、电流 */ \
  (((address) >= 114) && ((address) < 116)) ? ((address) - 114 + 25326) : /* 总有功功率 */ \
  (((address) >= 116) && ((address) < 118)) ? ((address) - 116 + 25334) : /* 总无功功率 */ \
  (((address) >= 118) && ((address) < 120)) ? ((address) - 118 + 25342) : /* 总视在功率 */ \
  (((address) >= 120) && ((address) < 124)) ? ((address) - 120 + 25350) : /* 功率因数、频率 */ \
  (((address) >= 124) && ((address) < 128)) ? ((address) - 124 + 25362) : /* 正/反向有功电能 */ \
  (((address) >= 128) && ((address) < 132)) ? ((address) - 128 + 25368) : /* 正/反向无功电能 */ \
  -1 \
)

/* 仅转发两份点表均有效的遥信。YX8/12/16..18/28/36/52/55/56
 * 至少一侧为备用/公共端，跳过；YX114..117微波报警在V0.18没有对应地址。
 * YX68公共端在V0.18中有明确定义（27668），按表保留。 */
#define Cem9000_Sweden_YX_INDEX(point) ( \
  (((point) >= 0) && ((point) < 8)) ? ((point) + 27600) : \
  (((point) >= 9) && ((point) < 12)) ? ((point) + 27600) : \
  (((point) >= 13) && ((point) < 16)) ? ((point) + 27600) : \
  (((point) >= 19) && ((point) < 28)) ? ((point) + 27600) : \
  (((point) >= 29) && ((point) < 52)) ? ((point) + 27600) : \
  (((point) >= 53) && ((point) < 55)) ? ((point) + 27600) : \
  (((point) >= 57) && ((point) < 114)) ? ((point) + 27600) : \
  -1 \
)

/***********************************************END************************************************************************************/
/**********************************************台达寄存器地址映射 **********************************************************************/  

#define Taida_INPUT_INDEX(pcs_index, address) ( \
  ((address) >= 30185-30001 && (address) < 30211-30001 ? ((address) - (30185-30001) + 2600+0+300*(pcs_index-1)) : /*Module1 Battery Voltage～PCS Rated Capacity*/\
  ((address) >= 30478-30001 && (address) < 30490-30001 ? ((address) - (30478-30001) + 2600+219+300*(pcs_index-1)) : /*Module1 FPGA Fault Word1~ Module1 Comm Fault Word1   */\
  ((address) >= 30578-30001 && (address) < 30590-30001 ? ((address) - (30578-30001) + 2600+233+300*(pcs_index-1)) : /*Module2 FPGA Fault Word1 ~ Module2 Comm Fault Word1  */\
  ((address) >= 30701-30001 && (address) < 30765-30001 ? ((address) - (30701-30001) + 2600+26+300*(pcs_index-1)) : /*模块1时放电量-高位  ~ 模块2统计时间-时 */\
  ((address) >= 30765-30001 && (address) < 30821-30001 ? ((address) - (30765-30001) + 2600+90+300*(pcs_index-1)) : /*AC Hourly Disc Capacity High Byte  ~ AC Statistical Hour */\
  ((address) >= 30011-30001 && (address) < 30045-30001 ? ((address) - (30011-30001) + 2600+152+300*(pcs_index-1)) : /*版本号 */\
  (address) == 30404-30001 ? 217+2600+300*(pcs_index-1) : /* Controller Fault Word1  */\
  (address) == 30492-30001 ? 231+2600+300*(pcs_index-1) : /* Module1 Warning Word1 */\
  (address) == 30493-30001 ? 232+2600+300*(pcs_index-1) : /* Module1 Warning Word2  */\
  (address) == 30592-30001 ? 245+2600+300*(pcs_index-1) : /* Module2 Warning Word1 */\
  (address) == 30593-30001 ? 246+2600+300*(pcs_index-1) : /* Module2 Warning Word2  */\
  (address) == 40522-40001 ? 186+2600+300*(pcs_index-1) : /* certification  */\
  (address) == 30408-30001 ? 218+2600+300*(pcs_index-1) : -1 ))))))/*Controller Warning Word1  */\
  )
#define Taida_5MW_INPUT_INDEX(pcs_index, address) ( \
  ((address) >= 30185-30001 && (address) < 30211-30001 ? ((address) - (30185-30001) + 5000+0+300*(pcs_index-1)) : /*Module1 Battery Voltage～PCS Rated Capacity*/\
  ((address) >= 30478-30001 && (address) < 30490-30001 ? ((address) - (30478-30001) + 5000+219+300*(pcs_index-1)) : /*Module1 FPGA Fault Word1~ Module1 Comm Fault Word1   */\
  ((address) >= 30578-30001 && (address) < 30590-30001 ? ((address) - (30578-30001) + 5000+233+300*(pcs_index-1)) : /*Module2 FPGA Fault Word1 ~ Module2 Comm Fault Word1  */\
  ((address) >= 30701-30001 && (address) < 30765-30001 ? ((address) - (30701-30001) + 5000+26+300*(pcs_index-1)) : /*模块1时放电量-高位  ~ 模块2统计时间-时 */\
  ((address) >= 30765-30001 && (address) < 30821-30001 ? ((address) - (30765-30001) + 5000+90+300*(pcs_index-1)) : /*AC Hourly Disc Capacity High Byte  ~ AC Statistical Hour */\
  ((address) >= 30011-30001 && (address) < 30045-30001 ? ((address) - (30011-30001) + 5000+152+300*(pcs_index-1)) : /*版本号 */\
  (address) == 30404-30001 ? 217+5000+300*(pcs_index-1) : /* Controller Fault Word1  */\
  (address) == 30492-30001 ? 231+5000+300*(pcs_index-1) : /* Module1 Warning Word1 */\
  (address) == 30493-30001 ? 232+5000+300*(pcs_index-1) : /* Module1 Warning Word2  */\
  (address) == 30592-30001 ? 245+5000+300*(pcs_index-1) : /* Module2 Warning Word1 */\
  (address) == 30593-30001 ? 246+5000+300*(pcs_index-1) : /* Module2 Warning Word2  */\
  (address) == 40522-40001 ? 186+5000+300*(pcs_index-1) : /* certification  */\
  (address) == 30408-30001 ? 218+5000+300*(pcs_index-1) : -1 ))))))/*Controller Warning Word1  */\
  )
#define Master_Taida_INPUT_INDEX(pcs_index, address) ( \
  ((address) >= 30011-30001 && (address) < 30045-30001 ? ((address) - (30011-30001) + 17000+0+300*(pcs_index-1)) : /*master -Local Controller Version 1 ~ mJ2FPGA Version 3  */\
  ((address) >= 30100-30001 && (address) < 30122-30001 ? ((address) - (30100-30001)+ 17000+34+300*(pcs_index-1)) : /*master -Grid Volt RS ~ Battery Total Current  */\
  ((address) >= 30125-30001 && (address) < 30132-30001 ? ((address) - (30125-30001) + 17000+56+300*(pcs_index-1)) : /*Rocof df/dt  ~ System State Word3  */\
  ((address) >= 30136-30001 && (address) < 30143-30001 ? ((address) - (30136-30001) + 17000+64+300*(pcs_index-1)) : /*System Efficient Mode1  ~ MV Integrated Fault Word1  */\
  ((address) >= 30237-30001 && (address) < 30241-30001 ? ((address) - (30237-30001) + 2600+200+300*(pcs_index-1)) : /*Slave System Fault Word1 ~Slave System Fault Word4 */\
  ((address) >= 30243-30001 && (address) < 30250-30001 ? ((address) - (30243-30001) + 2600+210+300*(pcs_index-1)) : /*Slave System Fault Word1 ~Slave System Fault Word4 */\
  ((address) >= 30148-30001 && (address) < 30160-30001 ? ((address) - (30148-30001) + 17000+73+300*(pcs_index-1)) : /*MV Status Word1  ~ Relay Ground Trip OCP Value  */\
  ((address) >= 30045-30001 && (address) < 30052-30001 ? ((address) - (30045-30001) + 17000+85+300*(pcs_index-1)) : /*PCS Time Year   ~ PCS Time mSec  */\
  (address) == 30134-30001 ? 17000+63+300*(pcs_index-1) : /* System Operation Status  */\
  (address) == 30144-30001 ? 17000+71+300*(pcs_index-1) : /* System Warning Word1  */\
  (address) == 30145-30001 ? 17000+72+300*(pcs_index-1) : /* System Warning Word2  */ -1 ))))))))/*master- Controller Warning Word1 */\
  )
  
#define Taida_HOLD_INDEX(pcs_index, address) ( \
  ((address) >= 40201-40001 && (address) < 40225-40001 ? ((address) - (40201-40001) + 12000+0+300*(pcs_index-1)) : /*master&slave-Grid Discharge Power Calibration k ~POI Theta offset Calibration b*/\
  ((address) >= 40011-40001 && (address) < 40018-40001 ? ((address) - (40011-40001) + 12000+26+300*(pcs_index-1)) : /*master&slave-PCS Time Year ~ PCS Time mSec*/\
  ((address) >= 40241-40001 && (address) < 40243-40001 ? ((address) - (40241-40001) + 12000+24+300*(pcs_index-1)) : -1 ))))/*master&slave-Set PCS Master ~Slave1 Control Enable*/\

#define Taida_5MW_HOLD_INDEX(pcs_index, address) ( \
  ((address) >= 40201-40001 && (address) < 40225-40001 ? ((address) - (40201-40001) + 15000+0+300*(pcs_index-1)) : /*master&slave-Grid Discharge Power Calibration k ~POI Theta offset Calibration b*/\
  ((address) >= 40011-40001 && (address) < 40018-40001 ? ((address) - (40011-40001) + 15000+26+300*(pcs_index-1)) : /*master&slave-PCS Time Year ~ PCS Time mSec*/\
  ((address) >= 40241-40001 && (address) < 40243-40001 ? ((address) - (40241-40001) + 15000+24+300*(pcs_index-1)) : -1 ))))/*master&slave-Set PCS Master ~Slave1 Control Enable*/\


#define Master_Taida_HOLD_INDEX(pcs_index, address) ( \
  ((address) >= 40113-40001 && (address) < 40117-40001 ? ((address) - (40113-40001) + 27000+109+300*(pcs_index-1)) : /*group-Apparent Power Reference～Island Frequency Reference*/\
  ((address) >= 40229-40001 && (address) < 40235-40001 ? ((address) - (40229-40001) + 27000+113+300*(pcs_index-1)) : /*group-DisDutyPRef ～Discharge power limitation */\
  (address) == 40105-40001 ? 50+27000+300*(pcs_index-1) : /*Master Bat1 Discharge Power Coef*/\
  (address) == 40103-40001 ? 103+27000+300*(pcs_index-1) : /*System ON/OFF Cmd*/\
  (address) == 40106-40001 ? 46+27000+300*(pcs_index-1) : /*Master Bat2 Discharge Power Coef */\
  (address) == 40107-40001 ? 47+27000+300*(pcs_index-1) : /*Slave Bat1 Discharge Power Coef */\
  (address) == 40109-40001 ? 48+27000+300*(pcs_index-1) : /*Master Bat2 Charge Power Coef  */\
  (address) == 40110-40001 ? 49+27000+300*(pcs_index-1) : /*Slave Bat1 charge Power Coef  */\
  (address) == 40111-40001 ? 52+27000+300*(pcs_index-1) : /*有功 */\
  (address) == 40112-40001 ? 53+27000+300*(pcs_index-1) : /*无功  */\
  (address) == 40108-40001 ? 51+27000+300*(pcs_index-1) : -1 )))/*Master Bat1 Charge Power Coef*/\




/*反响映射 */
  #define Taida_HOLD_ADDRESS(index) \
  (index) == 27107 ? 40111-40001 : /*主机1下有功功率*/\
  (index) == 27108 ?40112-40001 :/*主机1下无功功率*/\
  (index) == 27407 ? 40111-40001 : /*主机2下有功功率*/\
  (index) == 27408 ?40112-40001 :-1 /*主机2下无功功率*/\



/***********************************************END************************************************************************************/

/* ============================================================================================
 * 天合中压系统对外 Modbus TCP 通信协议 V0.16 (2026.10.19) —— Taida_PCS 点表地址表
 * --------------------------------------------------------------------------------------------
 * 点表对应的 PCS 为 Taida_PCS，故所有类型/常量统一使用 PCS_TAIDA_ 前缀，与 Trina、PE、
 * 上能等其它 PCS 的点表地址表区分，互不混用。
 *
 *   PCS_TAIDA_MODULE_INPUT_REG[n]  单台 PCS 只读输入寄存器    2600 + 300 * n
 *   PCS_TAIDA_GROUP_INPUT_REG[n]   PCS 组(主机)只读输入寄存器 17000 + 300 * n
 *   PCS_TAIDA_HOLD_REG[n]          单台 PCS 保持寄存器         12000 + 300 * n
 *   PCS_TAIDA_5MW_HOLD_REG[n]      5.5MW 一体机保持寄存器      15000 + 600 * n
 *   PCS_TAIDA_CTRL_REG[n]          PCS 组控制点               27000 + 300 * n
 *
 * 数组下标 n 从 0 开始，对应点表中的编号 #(n+1)；表中成员直接给出点表绝对地址，
 * 具体数值见 address_map.c。
 * ==========================================================================================*/

#define PCS_TAIDA_DEV_NUM_MAX       (4)     /* 点表：单台 PCS 编号 1~4      */
#define PCS_TAIDA_GROUP_NUM_MAX     (2)     /* 点表：PCS 组编号 1~2         */
#define PCS_TAIDA_5MW_DEV_NUM_MAX   (2)     /* 点表：5.5MW 一体机编号 1~2   */
#define PCS_TAIDA_CTRL_GROUP_STEP   (300)   /* 控制点组间步长：27000 -> 27300 */
#define PCS_TAIDA_HOLD_STEP         (300)   /* 单台保持寄存器步长：12000 -> 12300（5.5MW 段从机同此步长） */

/* 连续寄存器区间 [Start, End] */
typedef struct {
    INT16U Start;
    INT16U End;
} PCS_TAIDA_RANGE_ADDR_t;

/* 单台 PCS 只读输入寄存器（2600 段） */
typedef struct {
    INT16U Base;                             /* +0       单台PCS输入寄存器基址 2600 / 2900 / 3200 / 3500 */
    INT16U Battery1Power;                 /* +16   电池1功率 */
    INT16U Battery2Power;                 /* +17   电池2功率 */
    INT16U GridActivePower;               /* +18   电网有功功率 */
    INT16U GridReactivePower;             /* +19   电网无功功率 */
    INT16U GridVoltageRS;                 /* +20   电网电压 RS */
    INT16U GridVoltageST;                 /* +21   电网电压 ST */
    INT16U GridVoltageTR;                 /* +22   电网电压 TR */
    INT16U Module1HourlyDiscCapacityHigh; /* +26   模块1每小时放电容量高16位 */
    INT16U Module1HourlyDiscCapacityLow;  /* +27   模块1每小时放电容量低16位 */
    INT16U Module1HourlyCharCapacityHigh; /* +28   模块1每小时充电容量高16位 */
    INT16U Module1HourlyCharCapacityLow;  /* +29   模块1每小时充电容量低16位 */
    INT16U Module1TotalDiscCapacityHigh;  /* +42   模块1总放电容量高16位 */
    INT16U Module1TotalDiscCapacityLow;   /* +43   模块1总放电容量低16位 */
    INT16U Module1TotalCharCapacityHigh;  /* +44   模块1总充电容量高16位 */
    INT16U Module1TotalCharCapacityLow;   /* +45   模块1总充电容量低16位 */
    INT16U Module2DailyDiscCapacityHigh;  /* +66   模块2每日放电容量高16位 */
    INT16U Module2DailyDiscCapacityLow;   /* +67   模块2每日放电容量低16位 */
    INT16U Module2DailyCharCapacityHigh;  /* +68   模块2每日充电容量高16位 */
    INT16U Module2DailyCharCapacityLow;   /* +69   模块2每日充电容量低16位 */
    INT16U Module2DailyDiscCycleTimeHigh; /* +70   模块2每日放电循环时间高16位 */
    INT16U Module2DailyDiscCycleTimeLow;  /* +71   模块2每日放电循环时间低16位 */
    INT16U Module2DailyCharCycleTimeHigh; /* +72   模块2每日充电循环时间高16位 */
    INT16U Module2DailyCharCycleTimeLow;  /* +73   模块2每日充电循环时间低16位 */
    INT16U Module2TotalDiscCapacityHigh;  /* +74   模块2总放电容量高16位 */
    INT16U Module2TotalDiscCapacityLow;   /* +75   模块2总放电容量低16位 */
    INT16U Module2TotalCharCapacityHigh;  /* +76   模块2总充电容量高16位 */
    INT16U Module2TotalCharCapacityLow;   /* +77   模块2总充电容量低16位 */
    INT16U Module2TotalCharCycleTimeHigh; /* +80   模块2总充电循环时间高16位 */
    INT16U Module2TotalCharCycleTimeLow;  /* +81   模块2总充电循环时间低16位 */
    INT16U Module2DailyRuntimeHigh;       /* +82   模块2每日运行时间高16位 */
    INT16U Module2DailyRuntimeLow;        /* +83   模块2每日运行时间低16位 */
    INT16U Module2TotalRuntimeHigh;       /* +84   模块2总运行时间高16位 */
    INT16U Module2TotalRuntimeLow;        /* +85   模块2总运行时间低16位 */
    INT16U Module2StatisticalYear;        /* +86   模块2统计年份 */
    INT16U Module2StatisticalMonth;       /* +87   模块2统计月份 */
    INT16U Module2StatisticalDay;         /* +88   模块2统计日 */
    INT16U Module2StatisticalHour;        /* +89   模块2统计小时 */
    INT16U ACHourlyDiscCapacityHigh;      /* +90   AC每小时放电容量高16位 */
    INT16U ACHourlyDiscCapacityLow;       /* +91   AC每小时放电容量低16位 */
    INT16U ACHourlyCharCapacityHigh;      /* +92   AC每小时充电容量高16位 */
    INT16U ACHourlyCharCapacityLow;       /* +93   AC每小时充电容量低16位 */
    INT16U ACHourlyDiscCycleTimeHigh;     /* +94   AC每小时放电循环时间高16位 */
    INT16U ACHourlyDiscCycleTimeLow;      /* +95   AC每小时放电循环时间低16位 */
    INT16U ACHourlyCharCycleTimeHigh;     /* +96   AC每小时充电循环时间高16位 */
    INT16U ACHourlyCharCycleTimeLow;      /* +97   AC每小时充电循环时间低16位 */
    INT16U ACDailyDiscCapacityHigh;       /* +98   AC每日放电容量高16位 */
    INT16U ACDailyDiscCapacityLow;        /* +99   AC每日放电容量低16位 */
    INT16U ACDailyCharCapacityHigh;       /* +100  AC每日充电容量高16位 */
    INT16U ACDailyCharCapacityLow;        /* +101  AC每日充电容量低16位 */
    INT16U ACDailyDiscCycleTimeHigh;      /* +102  AC每日放电循环时间高16位 */
    INT16U ACDailyDiscCycleTimeLow;       /* +103  AC每日放电循环时间低16位 */
    INT16U ACDailyCharCycleTimeHigh;      /* +104  AC每日充电循环时间高16位 */
    INT16U ACDailyCharCycleTimeLow;       /* +105  AC每日充电循环时间低16位 */
    INT16U ACTotalDiscCapacityHigh;       /* +106  AC总放电容量高16位 */
    INT16U ACTotalDiscCapacityLow;        /* +107  AC总放电容量低16位 */
    INT16U ACTotalCharCapacityHigh;       /* +108  AC总充电容量高16位 */
    INT16U ACTotalCharCapacityLow;        /* +109  AC总充电容量低16位 */
    INT16U ACTotalDiscCycleTimeHigh;      /* +110  AC总放电循环时间高16位 */
    INT16U ACTotalDiscCycleTimeLow;       /* +111  AC总放电循环时间低16位 */
    INT16U ACTotalCharCycleTimeHigh;      /* +112  AC总充电循环时间高16位 */
    INT16U ACTotalCharCycleTimeLow;       /* +113  AC总充电循环时间低16位 */
    INT16U ACHourlyCapReactiveEnergyHigh; /* +114  AC每小时容性无功电量高16位 */
    INT16U ACHourlyCapReactiveEnergyLow;  /* +115  AC每小时容性无功电量低16位 */
    INT16U ACHourlyIndReactiveEnergyHigh; /* +116  AC每小时感性无功电量高16位 */
    INT16U ACHourlyIndReactiveEnergyLow;  /* +117  AC每小时感性无功电量低16位 */
    INT16U ACHourlyCapReactiveCycleTimeHigh;/* +118  AC每小时容性无功循环时间高16位 */
    INT16U ACHourlyCapReactiveCycleTimeLow;/* +119  AC每小时容性无功循环时间低16位 */
    INT16U ACHourlyIndReactiveCycleTimeHigh;/* +120  AC每小时感性无功循环时间高16位 */
    INT16U ACHourlyIndReactiveCycleTimeLow;/* +121  AC每小时感性无功循环时间低16位 */
    INT16U ACDailyCapReactiveEnergyHigh;  /* +122  AC每日容性无功电量高16位 */
    INT16U ACDailyCapReactiveEnergyLow;   /* +123  AC每日容性无功电量低16位 */
    INT16U ACDailyIndReactiveEnergyHigh;  /* +124  AC每日感性无功电量高16位 */
    INT16U ACDailyIndReactiveEnergyLow;   /* +125  AC每日感性无功电量低16位 */
    INT16U ACDailyCapReactiveCycleTimeHigh;/* +126  AC每日容性无功循环时间高16位 */
    INT16U ACDailyCapReactiveCycleTimeLow;/* +127  AC每日容性无功循环时间低16位 */
    INT16U ACDailyIndReactiveCycleTimeHigh;/* +128  AC每日感性无功循环时间高16位 */
    INT16U ACDailyIndReactiveCycleTimeLow;/* +129  AC每日感性无功循环时间低16位 */
    INT16U ACTotalCapReactiveEnergyHigh;  /* +130  AC总容性无功电量高16位 */
    INT16U ACTotalCapReactiveEnergyLow;   /* +131  AC总容性无功电量低16位 */
    INT16U ACTotalIndReactiveEnergyHigh;  /* +132  AC总感性无功电量高16位 */
    INT16U ACTotalIndReactiveEnergyLow;   /* +133  AC总感性无功电量低16位 */
    INT16U SoftwareVersionWord6;          /* +151  软件版本字6(版本号拼装位) */
    INT16U SystemFaultWord2;              /* +201  系统故障字2 */
    INT16U SystemFaultWord4;              /* +203  系统故障字4 */
    INT16U SystemWarningWord2;            /* +211  系统警告字2 */
    INT16U ControllerFaultWord1;          /* +217  控制器故障字1 */
    PCS_TAIDA_RANGE_ADDR_t Module1FaultWords;/* +219~+230 模块1 FPGA/IO/采样/通信故障字块 */
    PCS_TAIDA_RANGE_ADDR_t Module2FaultWords;/* +233~+244 模块2 FPGA/IO/采样/通信故障字块 */
} PCS_TAIDA_MODULE_INPUT_ADDR_t;

/* PCS 组(主机)只读输入寄存器（17000 段） */
typedef struct {
    INT16U Base;                             /* 组输入寄存器基址 17000 / 17300 */
    INT16U MasterPcsChargeDischargeStatus;   /* +32   主机充放电状态 */
    INT16U SlavePcsChargeDischargeStatus;    /* +33   从机充放电状态 */
    INT16U GridTotalActivePower;             /* +40   电网总有功功率 */
    INT16U GridTotalReactivePower;           /* +41   电网总无功功率 */
    INT16U GridPowerFactor;                  /* +42   电网功率因数 */
    INT16U GridFrequency;                    /* +43   电网频率 */
    INT16U POIActivePower;                   /* +50   并网点有功功率 */
    INT16U POIReactivePower;                 /* +51   并网点无功功率 */
    INT16U POIPowerFactor;                   /* +52   并网点功率因数 */
    INT16U BatteryTotalPower;                /* +54   电池总功率 */
    INT16U BatteryTotalCurrent;              /* +55   电池总电流 */
    INT16U SystemStateWord1;                 /* +60   系统状态字1 */
    INT16U SystemStateWord2;                 /* +61   系统状态字2 */
    INT16U SystemStateWord3;                 /* +62   系统状态字3 */
    INT16U SystemOperationStatus;            /* +63   系统运行状态 */
    INT16U SystemEfficientMode1;             /* +64   系统高效模式1 */
    INT16U SystemFaultWord1;                 /* +66   系统故障字1 */
    INT16U SystemFaultWord2;                 /* +67   系统故障字2 */
    INT16U SystemFaultWord3;                 /* +68   系统故障字3 */
    INT16U SystemFaultWord4;                 /* +69   系统故障字4 */
    INT16U SystemWarningWord2;               /* +72   系统警告字2 */
    INT16U DCChannel4Fault;                  /* +203  直流通道4 故障状态 */
    INT16U DCChannel4Status;                 /* +211  直流通道4 状态 */
} PCS_TAIDA_GROUP_INPUT_ADDR_t;

/* 单台 PCS 保持寄存器（12000 段） */
typedef struct {
    INT16U Base;                             /* 保持寄存器基址 12000 / 12300 / 12600 / 12900 */
    INT16U SetPCSMaster;                     /* +24   设置 PCS 主机 */
    INT16U Slave1ControlEnable;              /* +25   从机1 控制启用 */
    INT16U PcsTimeYearSet;                   /* +26   PCS 时间年份设置 */
    INT16U PcsTimeMonthSet;                  /* +27   PCS 时间月份设置 */
    INT16U PcsTimeDaySet;                    /* +28   PCS 时间日期设置 */
    INT16U PcsTimeHourSet;                   /* +29   PCS 时间小时设置 */
    INT16U PcsTimeMinSet;                    /* +30   PCS 时间分钟设置 */
    INT16U PcsTimeSecSet;                    /* +31   PCS 时间秒数设置 */
    INT16U PcsTimeMsecSet;                   /* +32   PCS 时间毫秒设置 */
} PCS_TAIDA_HOLD_ADDR_t;

/* 5.5MW 一体机 PCS 保持寄存器（15000 段） */
typedef struct {
    INT16U Base;                             /* 保持寄存器基址 15000 / 15600 */
    INT16U SetPCSMaster;                     /* +24   设置 PCS 主机 */
    INT16U Slave1ControlEnable;              /* +25   从机1 控制启用 */
    INT16U PcsTimeYearSet;                   /* +26   PCS 时间年份设置 */
    INT16U PcsTimeMonthSet;                  /* +27   PCS 时间月份设置 */
    INT16U PcsTimeDaySet;                    /* +28   PCS 时间日期设置 */
    INT16U PcsTimeHourSet;                   /* +29   PCS 时间小时设置 */
    INT16U PcsTimeMinSet;                    /* +30   PCS 时间分钟设置 */
    INT16U PcsTimeSecSet;                    /* +31   PCS 时间秒数设置 */
    INT16U PcsTimeMsecSet;                   /* +32   PCS 时间毫秒设置 */
} PCS_TAIDA_5MW_HOLD_ADDR_t;

/* PCS 组控制点（27000 段） */
typedef struct {
    INT16U Base;                             /* 控制点基址 27000 / 27300 */
    INT16U GroupReset;                       /* +0    Group Reset */
    INT16U GroupFaultClear;                  /* +1    Group Fault Clear */
    INT16U GroupToggle;                      /* +4    Group Toggle */
    INT16U GroupRocofCease;                  /* +5    Group Rocof Cease */
    INT16U PQMode;                           /* +16   PQ Mode */
    INT16U PFMode;                           /* +17   PF Mode */
    INT16U VoltVarMode;                      /* +18   Volt Var Mode */
    INT16U VoltWattMode;                     /* +19   Volt Watt Mode */
    INT16U FreqWattMode;                     /* +20   Freq Watt Mode */
    INT16U BlackStartMode;                   /* +21   Black Start Mode */
    INT16U StatisticsClear;                  /* +39   Statistics Clear */
    INT16U EnableCharge;                     /* +40   Enable Charge */
    INT16U EnableDischarge;                  /* +41   Enable Discharge */
    INT16U MVG3MCBClose;                     /* +42   MVG3 MCB Close */
    INT16U MVG3MCBOpen;                      /* +43   MVG3 MCB Open */
    INT16U ResetRelayTrip;                   /* +44   Reset Relay Trip */
    INT16U SetRelayOCPTh;                    /* +45   Set Relay OCP Th */
    INT16U MasterBat2DischargePowerCoef;     /* +46   主电池2 放电功率系数 */
    INT16U SlaveBat1DischargePowerCoef;      /* +47   从电池1 放电功率系数 */
    INT16U MasterBat2ChargePowerCoef;        /* +48   主电池2 充电功率系数 */
    INT16U SlaveBat1ChargePowerCoef;         /* +49   从电池1 充电功率系数 */
    INT16U MasterBat1DischargePowerCoef;     /* +50   主电池1 放电功率系数 */
    INT16U MasterBat1ChargePowerCoef;        /* +51   主电池1 充电功率系数 */
    INT16U ActivePowerReference;             /* +52   有功功率参考 */
    INT16U ReactivePowerReference;           /* +53   无功功率参考 */
    INT16U SystemOnOffCmd;                   /* +103  系统开/关命令 */
    INT16U PowerFactorReference;             /* +110  功率因数参考 */
    INT16U IslandVoltageReference;           /* +111  孤岛电压参考 */
    INT16U IslandFrequencyReference;         /* +112  孤岛频率参考 */
    INT16U DisDutyPRef;                      /* +113  放电任务功率参考 */
    INT16U EnDutyPRef;                       /* +114  充电任务功率参考 */
    INT16U HotStandbyEn;                     /* +115  热待机启用 */
} PCS_TAIDA_CTRL_ADDR_t;

/* 点表地址表实例，定义见 address_map.c */
extern const PCS_TAIDA_MODULE_INPUT_ADDR_t PCS_TAIDA_MODULE_INPUT_REG[PCS_TAIDA_DEV_NUM_MAX];
extern const PCS_TAIDA_GROUP_INPUT_ADDR_t  PCS_TAIDA_GROUP_INPUT_REG[PCS_TAIDA_GROUP_NUM_MAX];
extern const PCS_TAIDA_HOLD_ADDR_t         PCS_TAIDA_HOLD_REG[PCS_TAIDA_DEV_NUM_MAX];
extern const PCS_TAIDA_5MW_HOLD_ADDR_t     PCS_TAIDA_5MW_HOLD_REG[PCS_TAIDA_5MW_DEV_NUM_MAX];
extern const PCS_TAIDA_CTRL_ADDR_t         PCS_TAIDA_CTRL_REG[PCS_TAIDA_GROUP_NUM_MAX];

/* 控制点窗口判断：组 idx 的控制点为 [GroupReset, SetRelayOCPTh] */
#define PCS_TAIDA_CTRL_IS_POINT(a, idx) \
    ((a) >= PCS_TAIDA_CTRL_REG[(idx)].GroupReset && (a) <= PCS_TAIDA_CTRL_REG[(idx)].SetRelayOCPTh)

/* ============================================================================================
 * 同一份点表中的电池堆(Bank/BAU)只读输入寄存器 —— 38000 + 200 * n
 * --------------------------------------------------------------------------------------------
 * 该段设备是电池堆(BAU)，与上面的 Taida_PCS 不是同一类设备，故单独使用 BMS_ 前缀。
 * ==========================================================================================*/
#define BMS_DEV_NUM_MAX             (8)     /* 点表：电池堆编号 1~8 */

typedef struct {
    INT16U Base;                    /* 堆输入寄存器基址 38000 / 38200 / ... / 39400 */
    INT16U BankTotalVol;            /* +0   系统总电压(int32: +0~+1) */
    INT16U TotalSOC;                /* +3   系统SOC */
    INT16U BAU_SOH;                 /* +4   系统SOH */
    INT16U SystemInsulation;        /* +5   系统绝缘值 */
    INT16U ChgRemainCapacity;       /* +6   系统可充电量 */
    INT16U DchgRemainCapacity;      /* +7   系统可放电量 */
    INT16U SystemChgSOPDisplay;     /* +8   系统允许最大充电电流 */
    INT16U SystemDchgSOPDisplay;    /* +9   系统允许最大放电电流 */
    INT16U GridConnectCluNum;       /* +34  当前在网簇数 */
    INT16U SystemTotalCluNum;       /* +35  堆内总簇数 */
    INT16U DchgPower;               /* +37  堆可用放电功率 */
    INT16U ChgPower;                /* +38  堆可用充电功率 */
    INT16U ExternalFaultStatusSum;  /* +39  BMS系统故障汇总 */
    INT16U LegacyWarningWord1;     /* +40  BMS告警字1（V1.00 点表未定义，沿用旧布局） */
    INT16U LegacyWarningWord2;     /* +41  BMS告警字2（V1.00 点表未定义，沿用旧布局） */
    INT16U EntryBlackStartMode;     /* +60  进入黑启动模式 */
    INT16U BlackStartStatus;        /* +61  黑启动状态 */
    INT16U GridConnectStatus;       /* +62  一键并网开关状态 */
    INT16U SystemRunMode;           /* +71  系统运行模式 0正常/1禁充/2禁放/3待机/4停机 */
    INT16U ChgDchgStatus;           /* +72  系统充放电状态 0静置/1放电/2充电 */
    INT16U DryContactOutputStatus;  /* +73  BAU输出干接点状态 */
    INT16U Heartbeat;               /* +74  BMS心跳 */
} BMS_BAU_INPUT_ADDR_t;

extern const BMS_BAU_INPUT_ADDR_t BMS_BAU_INPUT_REG[BMS_DEV_NUM_MAX];

/* ============================================================================================
 * 同一份点表中的总系统(System)状态字 —— 单实例，与 PCS / BMS 表分开
 * ==========================================================================================*/
typedef struct {
    INT16U SystemStatusDetailed;         /* 106  系统总状态(详细版) */
    INT16U SystemFaultSummary;           /* 107  系统故障总 */
    INT16U SystemStatusSummary;          /* 108  系统总状态(精简版) */
    INT16U ActivePower;                  /* 125  有功功率(int32: 125~126) */
    INT16U StatusWord1_PcsComm10MW;      /* 218  PCS通信状态(10MW 一体机) */
    INT16U StatusWord3;                  /* 220  状态字3 */
    INT16U StatusWord6_PcsVersionCheck;  /* 223  PCS软件版本校验字 */
    INT16U StatusWord7_PcsSafetyCheck;   /* 224  PCS安规标准校验字 */
    INT16U StatusWord8_PcsModeCheck;     /* 225  PCS模式校验字 */
    INT16U StatusWord11_PcsComm5MW;      /* 228  PCS通信状态(5.5MW 一体机) */
} MVEMS_SYS_INPUT_ADDR_t;

extern const MVEMS_SYS_INPUT_ADDR_t MVEMS_SYS_INPUT_REG;

/* ============================================================================================
 * 同一份点表中的中压(MV)测控装置 —— 27100 段
 * ==========================================================================================*/
typedef struct {
    INT16U UPSStatus;                    /* 27109  UPS状态(int32: 27109~27110) */
    INT16U TripSummaryTotal;             /* 27600  事故总 */
    INT16U AlarmSummaryTotal;            /* 27601  测控告警总 */
    INT16U HeavyGasTrip;                 /* 27605  重瓦斯跳闸 */
    INT16U LightGasAlarm;                /* 27606  轻瓦斯报警 */
    INT16U OilUltraHighTempTrip;         /* 27607  油温超高温跳闸 */
    INT16U PressureReliefTrip;           /* 27610  压力释放阀跳闸 */
    INT16U UltraLowOilLevelTrip;         /* 27611  油位超低跳闸 */
    INT16U WindingOverTempTrip;          /* 27612  绕组超温跳闸 */
    INT16U AuxTransformerOverTempTrip;   /* 27613  辅助变超高温跳闸 */
    INT16U SmokeDetectorTrip;            /* 27619  烟感报警跳闸 */
    INT16U G02BreakerOpen;               /* 27631  G02柜断路器分闸位置 */
    INT16U G02BreakerClosed;             /* 27632  G02柜断路器合闸位置 */
    INT16U EmergencyStop;                /* 27650  应急跳闸 */
    INT16U Protection_Summary_Signal;    /* 25969  保护总信号 */
} MVEMS_MV_INPUT_ADDR_t;

extern const MVEMS_MV_INPUT_ADDR_t MVEMS_MV_INPUT_REG;

/* ============================================================================================
 * 天合中压系统对外 Modbus TCP 通信协议 V1.00 (2026.09.14) —— Trina_PCS 点表地址表
 * --------------------------------------------------------------------------------------------
 * 该点表对应的 PCS 为自研 Trina_PCS，地址分段与 Taida_PCS(V0.16) 不同：保持寄存器为 700 步长，
 * 控制量(开关机/复位/使能/功率给定等)都在保持寄存器段，27000 段只有直流通道命令。
 * 为避免与 PCS_TAIDA_ 系列混用，本段统一使用 PCS_TRINA_ 前缀。
 *
 *   PCS_TRINA_MODULE_INPUT_REG[n]  单台 PCS 只读输入寄存器  2600  + 300 * n
 *   PCS_TRINA_HOLD_REG[n]          单台 PCS 保持寄存器     12000 + 700 * n
 *   PCS_TRINA_GROUP_INPUT_REG[g]   PCS 组输入(直流通道)     17000 + 300 * g
 *   PCS_TRINA_CTRL_REG[n]          直流通道控制点         27000 + 300 * n
 *
 * 数组下标 n 从 0 开始，对应点表编号 #(n+1)（#1 主机、#2 从机、#3 主机、#4 从机）；
 * 成员直接给出点表绝对地址，具体数值见 address_map.c。
 * ==========================================================================================*/
#define PCS_TRINA_DEV_NUM_MAX       (4)     /* 点表：PCS 数量 1~4            */
#define PCS_TRINA_HOLD_STEP         (700)   /* 保持寄存器组间步长 12000->12700 */

/* 连续寄存器区间 [Start, End] */
typedef struct {
    INT16U Start;
    INT16U End;
} PCS_TRINA_RANGE_ADDR_t;

/* 单台 PCS 只读输入寄存器（2600 段） */
typedef struct {
    INT16U Base;                               /* +0   基址 2600 / 2900 / 3200 / 3500 */
    INT16U MsCtrlDspFaultTable4;               /* +26  MS控制DSP故障信息表4 */
    INT16U MsCtrlDspWarningTable1;             /* +27  MS控制DSP告警信息表1 */
    INT16U BlackStartState;                    /* +39  黑启动状态 */
    INT16U Branch1HourlyChargingCapHigh;       /* +64  支路1 时充电量 高16位 */
    INT16U Branch1HourlyChargingCapLow;        /* +65  支路1 时充电量 低16位 */
    INT16U Branch1HourlyDischargingCapHigh;    /* +66  支路1 时放电量 高16位 */
    INT16U Branch1HourlyDischargingCapLow;     /* +67  支路1 时放电量 低16位 */
    INT16U Branch1DailyChargingCapHigh;        /* +68  支路1 日充电量 高16位 */
    INT16U Branch1DailyChargingCapLow;         /* +69  支路1 日充电量 低16位 */
    INT16U Branch1DailyDischargingCapHigh;     /* +70  支路1 日放电量 高16位 */
    INT16U Branch1DailyDischargingCapLow;      /* +71  支路1 日放电量 低16位 */
    INT16U Branch1TotalChargingCapHigh;        /* +72  支路1 总充电量 高16位 */
    INT16U Branch1TotalChargingCapLow;         /* +73  支路1 总充电量 低16位 */
    INT16U Branch1TotalDischargingCapHigh;     /* +74  支路1 总放电量 高16位 */
    INT16U Branch1TotalDischargingCapLow;      /* +75  支路1 总放电量 低16位 */
    INT16U Branch2HourlyChargingCapHigh;       /* +76  支路2 时充电量 高16位 */
    INT16U Branch2HourlyChargingCapLow;        /* +77  支路2 时充电量 低16位 */
    INT16U Branch2HourlyDischargingCapHigh;    /* +78  支路2 时放电量 高16位 */
    INT16U Branch2HourlyDischargingCapLow;     /* +79  支路2 时放电量 低16位 */
    INT16U Branch2DailyChargingCapHigh;        /* +80  支路2 日充电量 高16位 */
    INT16U Branch2DailyChargingCapLow;         /* +81  支路2 日充电量 低16位 */
    INT16U Branch2DailyDischargingCapHigh;     /* +82  支路2 日放电量 高16位 */
    INT16U Branch2DailyDischargingCapLow;      /* +83  支路2 日放电量 低16位 */
    INT16U Branch2TotalChargingCapHigh;        /* +84  支路2 总充电量 高16位 */
    INT16U Branch2TotalChargingCapLow;         /* +85  支路2 总充电量 低16位 */
    INT16U Branch2TotalDischargingCapHigh;     /* +86  支路2 总放电量 高16位 */
    INT16U Branch2TotalDischargingCapLow;      /* +87  支路2 总放电量 低16位 */
    INT16U SystemRunStatus;                    /* +100 系统状态(停机/运行/故障/降额/主控通信丢失) */
    INT16U GridVoltageRSPCC;                   /* +103 电网电压RS PCC */
    INT16U GridVoltageSTPCC;                   /* +104 电网电压ST PCC */
    INT16U GridVoltageTRPCC;                   /* +105 电网电压TR PCC */
    INT16U GridFrequencyPCC;                   /* +106 电网频率 PCC */
    INT16U DCPowerTotal;                       /* +110 直流功率(总) */
    INT16U ActivePowerTotalPCC;                /* +111 有功功率(总) PCC */
    INT16U ReactivePowerTotalPCC;              /* +112 无功功率(总) PCC */
    INT16U ApparentPowerTotalPCC;              /* +113 视在功率(总) PCC */
    INT16U PowerFactorTotalPCC;                /* +114 功率因数(总) PCC */
    INT16U BatteryCurrentTotal;                /* +115 电池电流(总) */
    INT16U BatteryPowerJ1;                     /* +141 电池功率(J1)/支路1功率 */
    INT16U BatteryPowerJ2;                     /* +148 电池功率(J2)/支路2功率 */
} PCS_TRINA_MODULE_INPUT_ADDR_t;

/* 单台 PCS 保持寄存器（12000 段，步长 700） */
typedef struct {
    INT16U Base;                               /* 基址 12000 / 12700 / 13400 / 14100 */
    INT16U ParallelMode;                       /* +0   并联模式 */
    INT16U PowerOnCommand;                     /* +1   开机指令 */
    INT16U OnGridOffGridConfig;                /* +2   并离网设置 */
    INT16U OperationMode;                      /* +3   运行模式 */
    INT16U ActivePowerSetpoint;                /* +4   有功功率给定 */
    INT16U DischargeEnable;                    /* +5   放电使能 */
    INT16U ChargeEnable;                       /* +6   充电使能 */
    INT16U FaultClear;                         /* +7   故障清除 */
    INT16U Reset;                              /* +8   复位 */
    INT16U MasterSlaveModeSetting;             /* +9   主从机设置 */
    INT16U SingleBusEnable;                    /* +10  单支路使能 */
    INT16U MaxChargingVoltage;                 /* +11  充电电压上限 */
    INT16U MinDischargingVoltage;              /* +12  放电电压下限 */
    INT16U MaxChargingCurrent;                 /* +13  最大充电电流 */
    INT16U MaxDischargingCurrent;              /* +14  最大放电电流 */
    INT16U BlackStartEnable;                   /* +27  黑启动使能 */
    INT16U ZeroPowerStandbyEnable;             /* +28  0功率待机使能 */
    INT16U ActiveReactivePowerPriority;        /* +29  有功无功优先 */
    INT16U ApparentPowerLimit;                 /* +30  视在功率限幅 */
    INT16U DischargePowerLimit;                /* +31  放电功率限幅 */
    INT16U ChargePowerLimit;                   /* +32  充电功率限幅 */
    INT16U MasterDischargeDistCoef;            /* +33  主机放电分配系数 */
    INT16U MasterChargeDistCoef;               /* +34  主机充电分配系数 */
    INT16U BranchDischargeDistCoef;            /* +35  支路放电分配系数 */
    INT16U BranchChargeDistCoef;               /* +36  支路充电分配系数 */
    INT16U Droop_PQ;                           /* +37  下垂模式下转PQ模式 */
    INT16U ReactivePowerEnable;                /* +61  无功使能 */
    INT16U FixedPowerFactor;                   /* +62  固定功率因数 */
    INT16U FixedCosphi;                        /* +63  固定cosphi */
    INT16U FixedReactivePowerPercent;          /* +64  固定无功功率百分比(无功模式二) */
    INT16U UpgradeSignal;                      /* +77  升级信号 */
    INT16U FactoryReset;                       /* +78  PCS恢复出厂设置 */
    INT16U SystemTimeSec;                      /* +94  系统时间-秒 */
    INT16U SystemTimeMin;                      /* +95  系统时间-分 */
    INT16U SystemTimeHour;                     /* +96  系统时间-时 */
    INT16U SystemTimeDay;                      /* +97  系统时间-日 */
    INT16U SystemTimeMonth;                    /* +98  系统时间-月 */
    INT16U SystemTimeYear;                     /* +99  系统时间-年 */
    INT16U CountryCode;                        /* +100 国家码/地区码 */
    PCS_TRINA_RANGE_ADDR_t J1CurrentRange;     /* +440 ~ +461 J1 板桥臂/PCC 电流段 */
    PCS_TRINA_RANGE_ADDR_t J2CurrentRange;     /* +480 ~ +501 J2 板桥臂/PCC 电流段 */
} PCS_TRINA_HOLD_ADDR_t;

#define PCS_TRINA_GROUP_NUM_MAX     (2)     /* 点表：PCS 组 1~2（每组 2 台 PCS、4 个直流通道） */

/* PCS 组输入寄存器（17000 段，直流通道汇总） */
typedef struct {
    INT16U Base;                               /* 基址 17000 / 17300 */
    PCS_TRINA_RANGE_ADDR_t DCChannelFault;     /* +200 ~ +203  直流通道1~4 故障状态 */
    PCS_TRINA_RANGE_ADDR_t DCChannelAlarm;     /* +204 ~ +207  直流通道1~4 告警状态 */
    PCS_TRINA_RANGE_ADDR_t DCChannelStatus;    /* +208 ~ +211  直流通道1~4 状态 */
    PCS_TRINA_RANGE_ADDR_t DCChannelIslandFreq; /* +212 ~ +215  直流通道1~4 孤岛频率 */
    PCS_TRINA_RANGE_ADDR_t DCChannelGridVolt;  /* +216 ~ +227  直流通道1~4 电网电压 RS/ST/TR */
    PCS_TRINA_RANGE_ADDR_t DCChannelActivePower; /* +228 ~ +231  直流通道1~4 有功功率 */
    PCS_TRINA_RANGE_ADDR_t DCChannelBlackStartRef; /* +236 ~ +239  直流通道1~4 黑启动状态反馈 */
    INT16U LegacyMasterChgDchgStatus;          /* +32  主机充放电状态（V1.00 点表未定义，沿用旧布局） */
    INT16U LegacySlaveChgDchgStatus;           /* +33  从机充放电状态（V1.00 点表未定义，沿用旧布局） */
    INT16U LegacyGridPowerFactor;              /* +42  电网功率因数（V1.00 点表未定义，沿用旧布局） */
    INT16U LegacyGridFrequency;                /* +43  电网频率（V1.00 点表未定义，沿用旧布局） */
    INT16U LegacySystemStateWord1;             /* +60  系统状态字1（V1.00 点表未定义，沿用旧布局） */
    INT16U LegacySystemOperationStatus;        /* +63  系统运行状态（V1.00 点表未定义，沿用旧布局） */
} PCS_TRINA_GROUP_INPUT_ADDR_t;

/* PCS 组输入寄存器（17000 段，直流通道汇总） */
typedef struct {
    INT16U Base;                               /* 基址 27000*/
    PCS_TRINA_RANGE_ADDR_t Droop_pq;     /* +212 ~ +215  下垂转PQ */
} PCS_TRINA_GROUP_HOLD_ADDR_t;
/* PCS 直流通道控制点（27000 段，按 PCS 编号 27000 + 300*n） */
typedef struct {
    INT16U Base;                               /* 基址 27000 / 27300 / 27600 / 27900 */
    INT16U DCChannel1OnOffCmd;                 /* +200  直流通道1 开/关命令 */
    INT16U DCChannel2OnOffCmd;                 /* +201  直流通道2 开/关命令 */
    INT16U DCChannel3OnOffCmd;                 /* +202  直流通道3 开/关命令 */
    INT16U DCChannel4OnOffCmd;                 /* +203  直流通道4 开/关命令 */
    INT16U DCChannel1ActivePowerRef;           /* +204  直流通道1 参考有功功率 */
    INT16U DCChannel2ActivePowerRef;           /* +205  直流通道2 参考有功功率 */
    INT16U DCChannel3ActivePowerRef;           /* +206  直流通道3 参考有功功率 */
    INT16U DCChannel4ActivePowerRef;           /* +207  直流通道4 参考有功功率 */
    INT16U DCChannel1ReactivePowerRef;         /* +208  直流通道1 参考无功功率 */
    INT16U DCChannel2ReactivePowerRef;         /* +209  直流通道2 参考无功功率 */
    INT16U DCChannel3ReactivePowerRef;         /* +210  直流通道3 参考无功功率 */
    INT16U DCChannel4ReactivePowerRef;         /* +211  直流通道4 参考无功功率 */
} PCS_TRINA_CTRL_ADDR_t;

/* 点表地址表实例，定义见 address_map.c */
extern const PCS_TRINA_MODULE_INPUT_ADDR_t PCS_TRINA_MODULE_INPUT_REG[PCS_TRINA_DEV_NUM_MAX];
extern const PCS_TRINA_HOLD_ADDR_t         PCS_TRINA_HOLD_REG[PCS_TRINA_DEV_NUM_MAX];
extern const PCS_TRINA_GROUP_INPUT_ADDR_t  PCS_TRINA_GROUP_INPUT_REG[PCS_TRINA_GROUP_NUM_MAX];
extern const PCS_TRINA_GROUP_HOLD_ADDR_t   PCS_TRINA_GROUP_HOLD_REG[PCS_TRINA_GROUP_NUM_MAX];
extern const PCS_TRINA_CTRL_ADDR_t         PCS_TRINA_CTRL_REG[PCS_TRINA_DEV_NUM_MAX];

/* 保持寄存器 J1/J2 板电流段判定：idx 为 PCS 编号（0~3） */
#define PCS_TRINA_IS_J1_CURRENT(a, idx) \
    ((a) >= PCS_TRINA_HOLD_REG[(idx)].J1CurrentRange.Start && \
     (a) <= PCS_TRINA_HOLD_REG[(idx)].J1CurrentRange.End)
#define PCS_TRINA_IS_J2_CURRENT(a, idx) \
    ((a) >= PCS_TRINA_HOLD_REG[(idx)].J2CurrentRange.Start && \
     (a) <= PCS_TRINA_HOLD_REG[(idx)].J2CurrentRange.End)
/**********************************************UPS寄存器地址映射 **********************************************************************/  
#define UPS_INPUT_INDEX(address) ( \
    ((address) == 208) ? 27113 : /* UPS Mode inquiry */ \
    ((address) == 879) ? 27114 : /* Unit Battery Capacity(AH) */ \
    ((address) == 675) ? 27115 : /* Fault kind */ \
    ((address) == 1010) ? 27116 : /* Battery Piece Number */ \
    ((address) == 1264) ? 27126 : /* Alarm word 1 */ \
    ((address) == 178) ? 27107 : /* P Battery voltage */ \
    ((address) == 0) ? 27129 : /* P Battery voltage */ \
    (((address) >= 1012) && ((address) < 1016)) ? ((address) - 1012 + 27117) : /*Input phase~Output rated VA*/\
    (((address) >= 1017) && ((address) < 1019)) ? ((address) - 1017 + 27121) : /*Input phase~Output rated VA*/\
    (((address) >= 169) && ((address) < 176)) ? ((address) - 169 + 27100) : /*Battery current~Battery remain time*/\
    (((address) >= 180) && ((address) < 183)) ? ((address) - 180 + 27108) : /*UPS Internal Temperature~Ups status*/\
    (((address) >= 191) && ((address) < 193)) ? ((address) - 191 + 27111) : /*Battery capacity~Battery remain time*/\
    (((address) >= 3) && ((address) < 6)) ? ((address) - 3 + 27130) :  /*warning0~5*/\
    (((address) >= 1162) && ((address) < 1166)) ? ((address) - 1162 + 27123) : /*Battery Voltage~Rating Output Frequency*/\
    -1 \
)


/***********************************************END************************************************************************************/









/***********************************************协能BMS寄存器地址映射********************************************************************/

// ================= HOLD BANK Index宏 ===================
//xieneng BMS实际设备地址映射对外地址
#define HOLD_B_INDEX(bank_index, address) \
((address) >= 1 && (address) < 9 ? ((address-1) + 35001+300*(bank_index-1)) : /*一键并网开关～时间-秒*/\
  (address) == 11 ? 35011+300*(bank_index-1) : /*复位*/\
  (address) == 12800 ? 35274+300*(bank_index-1) : /*设置rack起停*/\
  ((address) >= 256 && (address) < 274 ? ((address-256) + 35256+300*(bank_index-1)) : -1))/*绝缘采样开关～客户端3本地端口号   */\


  // ================= BANK Index宏 ===================
  //xieneng BMS实际设备地址映射对外地址
 #define INPUT_B_INDEX(bank_index, address) \
  (address) == 4097 ? 0+30000+100*(bank_index-1): /*一键并网开关状态*/\
  (address) == 4098 ? 1+30000+100*(bank_index-1) : /*Rack起停状态*/\
  ((address) >= 4101 && (address) < 4141 ? ((address) - 4101 + 2+30000+100*(bank_index-1)) : /*Rack在线状态~RACK起停状态*/\
  ((address) >= 16426 && (address) < 16435 ? ((address) - 16426 + 0+48000+50*(bank_index-1)) : /*水泵*/\
  ((address) >= 16437 && (address) < 16435 ? ((address) - 16469 + 0+49000+50*(bank_index-1)) : /*空调*/\
  (address) == 16558 ? 0+49000+20*(bank_index-1) : /*CO 浓度*/\
  (address) == 16560 ? 1+49000+20*(bank_index-1) : /*CO 故障和告警*/\
  (address) == 16562 ? 2+49000+20*(bank_index-1) : /*CO 监控器状态*/\
  (address) == 16566 ? 3+49000+20*(bank_index-1) : /*CO 可燃气体探测*/\
  (address) == 16567 ? 3+49400+20*(bank_index-1) : /*气表读数*/\
  ((address) >= 16569 && (address) < 16576 ? ((address) - 16569 + 1+49400+20*(bank_index-1)) : /*气体*/\
  (address) == 16577 ? 0+49800+20*(bank_index-1) : /*气表读数*/\
  ((address) >= 4144 && (address) < 4155 ? ((address) - 4144 + 43+30000+100*(bank_index-1)) : /*累积充电电量高位～Rack故障汇总*/\
  ((address) >= 16592 && (address) < 16594 ? ((address) - 16592 + 0+49900+20*(bank_index-1)) : /*TCP扩展IO*/\
  (address) == 4143 ? 42+30000+100*(bank_index-1) : -1 ))))))/*Rack在线状态2*/\

  #define INPUT_B_test_INDEX(bank_index, address) \
  (address) == 4097 ? 62+28000+200*(bank_index-1): /*一键并网开关状态*/\
  (address) == 4098 ? 64+28000+200*(bank_index-1) : /*Rack起停状态*/\
  (address) == 4102 ? 71+28000+200*(bank_index-1) : /*系统运行状态*/\
  (address) == 4103 ? 72+28000+200*(bank_index-1) : /*系统充放电状态*/\
  (address) == 4107 ? 40+28000+200*(bank_index-1) : /*系统一级报警汇总*/\
  (address) == 4106 ? 41+28000+200*(bank_index-1) : /*系统二级报警汇总*/\
  (address) == 4105 ? 42+28000+200*(bank_index-1) : /*系统三级报警汇总（预警）*/\
  (address) == 4104 ? 39+28000+200*(bank_index-1) : /*BAU报警状态*/\
  (address) == 16485 ? 51+28000+200*(bank_index-1) : /*Rack一级报警汇总2*/\
  ((address) >= 4110 && (address) < 4137 ? ((address) - 4110 + 3+28000+200*(bank_index-1)) : /*系统SOC~系统平均温度*/\
  (address) == 4108 ? 0+28000+200*(bank_index-1) : /*系统总电压*/\
  (address) == 4109 ? 2+28000+200*(bank_index-1) : /*系统总电流*/\
  (address) == 4153 ? 74+28000+200*(bank_index-1) : /*BMS心跳*/\
  ((address) >= 4151 && (address) < 4153 ? ((address) - 4151 + 37+28000+200*(bank_index-1)) : /*堆可用放电功率~堆可用充电功率*/\
  (address) == 4154 ? 50+28000+200*(bank_index-1) : -1))/*Rack 故障汇总*/\
  
 // ================= RACK Index宏 ===================
 #define INPUT_R_INDEX(bank_index,rack_index,address) \
  ((address) >= 8193 && (address) < 8226 ? ((address) - 8193)+31000+(rack_index-1)*100+(bank_index-1)*1500 : /*Rack运行状态～Rack高压箱温度2*/\
  ((address) >= 8236 && (address) < 8239 ? ((address) - 8236 + 33+31000+(rack_index-1)*100+(bank_index-1)*1500) : /*Rack告警*/\
  (address) == 12812 ? 36+31000+(rack_index-1)*100+(bank_index-1)*1500 : -1))/*均衡开关*/\

 // ================= CELL Index宏 ===================
 #define INPUT_C_INDEX(rack_index,address) \
 ((address) >= 8736 && (address) < 8864 ? ((address) - 8736)+(rack_index-1)*1152 : /*Rack正～负极柱温度*/\
  ((address) >= 9216 && (address) < 10240 ? ((address) - 9216 + 128)+(rack_index-1)*1152 : -1))/*Rack单体电池电压～温度*/\

/***********************************************END********************************************************************/
/******************************************************G2pro寄存器地址映射 ***********************************************/
  // BMS实际设备地址映射对外地址
#define G2pro_INPUT_B_INDEX(bank_index, address) \
( \
  ((address) >= 0x2001 && (address) <= 0x2003) ? ((address) - 0x2001 + 0 + 38000 + 200 * (bank_index - 1)) : /*堆电压~堆电流*/ \
  ((address) == 0x2029) ? (3 + 38000 + 200 * (bank_index - 1)) : /*SOC*/ \
  ((address) == 0x202B) ? (4 + 38000 + 200 * (bank_index - 1)) : /*SOH*/ \
  ((address) == 0x2004) ? (5 + 38000 + 200 * (bank_index - 1)) : /*绝缘电阻*/ \
  ((address) >= 0x204C && (address) <= 0x204F) ? ((address) - 0x204C + 6 + 38000 + 200 * (bank_index - 1)) : /*可充电能量~可放电电流*/ \
  ((address) == 0x2076) ? (10 + 38000 + 200 * (bank_index - 1)) : /*簇间电流差*/ \
  ((address) >= 0x207B && (address) <= 0x2084) ? ((address) - 0x207B + 11 + 38000 + 200 * (bank_index - 1)) : /*簇间总压差～平均温度*/ \
  ((address) >= 0x2086 && (address) <= 0x208E) ? ((address) - 0x2086 + 21 + 38000 + 200 * (bank_index - 1)) : /*簇间总压差～平均温度*/ \
  ((address) >= 0x20CE && (address) <= 0x20D1) ? ((address) - 0x20CE + 30 + 38000 + 200 * (bank_index - 1)) : /*累计充电能量～累计放电能量*/ \
  ((address) >= 0x2005 && (address) <= 0x2007) ? ((address) - 0x2005 + 34 + 38000 + 200 * (bank_index - 1)) : /*当前在网簇数~最小并网簇数*/ \
  ((address) >= 0x2050 && (address) <= 0x2051) ? ((address) - 0x2050 + 37 + 38000 + 200 * (bank_index - 1)) : /*最大运行放电功率~最大运行充电功率*/ \
  ((address) == 0x1001) ? (39 + 38000 + 200 * (bank_index - 1)) : /*对外故障状态*/ \
  ((address) == 0x1011) ? (60 + 38000 + 200 * (bank_index - 1)) : /*黑启动模式*/ \
  ((address) == 0x1012) ? (61 + 38000 + 200 * (bank_index - 1)) : /*当前黑启动状态*/ \
  ((address) == 0x1004) ? (62 + 38000 + 200 * (bank_index - 1)) : /*一键并网状态*/ \
  ((address) >= 0x1002 && (address) <= 0x1003) ?  ((address) - 0x1002 + 71 + 38000 + 200 * (bank_index - 1)) : /*系统运行模式~系统充放电状态*/ \
  ((address) >= 0x0621 && (address) <= 0x0622) ? (73 + 38000 + 200 * (bank_index - 1)) : /*干接点状态*/ \
  ((address) == 0x2008) ? (74 + 38000 + 200 * (bank_index - 1)) : /*BAU心跳信号*/ \
  ((address) >= 1790 && (address) <= 1792) ? ((address) - 1790 + 0 + 64000 + 20 * (bank_index - 1)) : /*氢气*/ \
  -1 \
)

 // ================= RACK Index宏 ===================
 #define G2pro_INPUT_R_INDEX(bank_index,rack_index,address) \
  ((address) >= 528 && (address) < 642 ? ((address) - 528)+31000+(rack_index-1)*200+(bank_index-1)*3000 : /*单体最高电压~模组电压过低停机时电压值*/\
  ((address) >= 16 && (address) < 56 ? ((address) - 16 + 114+31000+(rack_index-1)*200+(bank_index-1)*3000) : /*功能安全告警详细故障~电池系统一级报警信息（Warning）*/\
  -1))\

  // ================= CELL Index宏 ===================
 #define G2pro_INPUT_C_INDEX(rack_index,address) \
 ((address) >= 5121 && (address) < 5633 ? ((address) - 5121)+128+(rack_index-1)*2000 : /*单体电压*/\
 ((address) >= 6657 && (address) < 6722 ? ((address) - 6657)+1152+(rack_index-1)*2000 : /*铜牌温度*/\
 ((address) >= 8769 && (address) < 8773 ? ((address) - 8769)+0+(rack_index-1)*2000 : /*PACK正极柱温度*/\
 ((address) >= 8897 && (address) < 8901 ? ((address) - 8897)+64+(rack_index-1)*2000 : /*PACK负极柱温度*/\
  ((address) >= 6145 && (address) < 6657 ? ((address) - 6145 + 640)+(rack_index-1)*2000 : -1)))))/*电池温度*/\
 
  // ================= HOLD BANK Index宏 ===================
//xieneng BMS实际设备地址映射对外地址
#define G2pro_HOLD_B_INDEX(bank_index, address) \
((address) >= 12289 && (address) < 12317 ? ((address-12289) + 35001+300*(bank_index-1)) : /*一键并网开关～时间-设置Rack启用/停用*/\
((address) >= 21037 && (address) < 21045 ? ((address-21037) + 35032+300*(bank_index-1)) : /*NTP时区～BAU-EMS 通讯故障（心跳）*/\
((address) >= 12357 && (address) < 12360 ? ((address-12357) + 35029+300*(bank_index-1)) : -1)))/*本地/远程控制切换～一键复归   */\

/************************************************************************************************** */
 #define GET_INPUT(address) \
 ({ \
     (address >= 0 && address < 65535) ? \
     RegM.Input[address] : 0; \
 })
 
 #define SET_INPUT(address, val) \
 ({ \
     if (address >= 0 && address < 65535) \
      RegM.Input[address] = (val); \
 })

 #define GET_C_INPUT(bank_index,address) \
 ({ \
     (address >= 0 && address < R_INPUT_SIZE+(RACK_SIZE-1)*1152 && bank_index >= 0 && bank_index < BANK_SIZE) ? \
     RankM.Input[bank_index][address] : 0; \
 })
 
 #define SET_C_INPUT(bank_index,address, val) \
 ({ \
     if (address >= 0 && address < R_INPUT_SIZE+(RACK_SIZE-1)*1152 && bank_index >= 0 && bank_index < BANK_SIZE) \
         RankM.Input[bank_index][address] = (val); \
 })

extern volatile RegistersModel *const pRegM;
extern volatile RankModel *const pRankM;





 #endif //ADDRESS_MAP_H
 