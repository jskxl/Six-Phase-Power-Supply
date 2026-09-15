#ifndef __POWER_COM_H
#define __POWER_COM_H
#include "stm32f10x.h"

#ifndef u8
#define u8 unsigned char
#endif
#ifndef u16
#define u16 unsigned int
#endif
#ifndef u32
#define u32 unsigned long
#endif

/* 帧头 */
#define PC_SYNC0 0xAA
#define PC_SYNC1 0x55

/* 功能码 */
#define PC_FUNC_SET_CH   0x01   /* 设置单通道(电压+电流+使能)  D->P  DATA=6 */
#define PC_FUNC_SET_MASK 0x02   /* 设置输出使能掩码            D->P  DATA=1 */
#define PC_FUNC_SET_VOLT 0x03   /* 设置单通道电压              D->P  DATA=3 */
#define PC_FUNC_SET_CURR 0x04   /* 设置单通道电流              D->P  DATA=3 */
#define PC_FUNC_QUERY    0x05   /* 查询测量数据                D->P  DATA=0 */
#define PC_FUNC_SET_ALL  0x06   /* 设置全部通道电压+电流       D->P  DATA=24 */
#define PC_FUNC_CAL_ZERO 0x07   /* 电流零偏校准(清0)         D->P  DATA=1 */
#define PC_FUNC_MEASURE  0x81   /* 测量数据上报                P->D  DATA=28 */
#define PC_FUNC_ACK      0x82   /* 命令ACK应答                 P->D  DATA=2 */
#define PC_FUNC_ALARM    0x83   /* 报警/状态上报               P->D  DATA=2 */

/* ACK 状态 */
#define PC_ACK_OK      0x00
#define PC_ACK_CH_ERR  0x01
#define PC_ACK_RANGE   0x02
#define PC_ACK_CRC     0x03
#define PC_ACK_EXEC    0x04

typedef struct
{
	u16 in_v;       /* 输入电压 0.01V */
	u16 out_v[6];   /* 输出电压 0.01V */
	u16 out_i[6];   /* 输出电流 0.001A */
	u8  mask;       /* 实际输出状态位图 bit0=ch1..bit5=ch6 */
	u8  seq;
	u8  seq_miss;   /* 丢帧计数 */
	u8  has_data;   /* 已收到有效测量帧 */
	u8  lost;       /* 链路中断 */
	u8  new_data;   /* 新测量帧待处理 */
	u8  alarm;      /* 报警位图 */
	u8  fault;      /* 故障位图 */
	u8  alarm_new;  /* 新报警待处理 */
	u8  ack_flag;   /* 收到ACK待处理 */
	u8  ack_func;
	u8  ack_status;
	u8  cmd_busy;   /* 有命令等待ACK */
	u8  cmd_fail;   /* 重传3次失败 */
	u32 last_tick;  /* 最近测量帧时间 */
	u32 query_tick; /* 最近一次恢复查询时间 */
} PC_MEAS_T;

extern PC_MEAS_T pc_meas;

void PowerCom_Init(void);                         /* USART3: PB10(TX) PB11(RX), 115200 8N1 */
void PC_Task(void);                               /* 主循环周期调用: ACK超时重传/链路超时/恢复查询 */
void PC_SendCmd(u8 func, const u8 *data, u8 len); /* 组帧发送并入队等待ACK(可重传) */
void PW_SetChannelEx(u8 ch, u16 i_set, u8 on);
void PW_SetChannel(u8 ch);                        /* 0x01: 下发单通道电压+电流+使能 */
void PW_SetVolt(u8 ch);                           /* 0x03: 下发单通道电压 */
void PW_SetCurr(u8 ch);                           /* 0x04: 下发单通道电流 */
void PW_CalZero(u8 ch);                           /* 0x07: 电流零偏校准 ch=0全部/1~6 */

#endif
