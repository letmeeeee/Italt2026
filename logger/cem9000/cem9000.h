#ifndef CEM9000_H
#define CEM9000_H
void Cem9000_Task(const char *arg);


#define Cem9000_SLAVE_ADDR        (1)
#define Rec_BUFF_LEN        (300)

#define Cem9000_Addr1      (0)  
#define cem9000size1      (100)    //cem9000遥测数据点

#define Cem9000_Addr4      (100)  
#define cem9000size4      (32)    //cem9000遥测数据点

#define Cem9000_Addr2      (1000)  
#define cem9000size2      (8)    //cem9000遥信数据点

#define Cem9000_Addr3      (2000)  
#define cem9000size3      (2)    //cem9000遥控数据点

#define high_vol_switch_addr 2000 //远方分闸地址


#define addresslimit     2020

//芬兰协议

#define cem9000_finland_size1      (100)    //cem9000遥测数据点

#define cem9000_finland_size2      (9)    //cem9000遥信数据点



//英国协议：起始地址与芬兰相同，遥测0..203，遥信1000..1028。
#define cem9000_uk_size4          (104)  //第二段遥测100..203

#define cem9000_uk_size2          (29)   //遥信1000..1028，共454位



#endif