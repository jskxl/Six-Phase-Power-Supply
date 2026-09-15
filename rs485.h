#ifndef __RS485_H
#define __RS485_H
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


#define RS485_DEF_ADDR     1
#define RS485_DEF_BAUD_IDX 1

extern const u32 rs485_bauds[6];

extern u8 rs485_addr;
extern u8 rs485_baud_idx;
extern volatile u32 rs485_rx_tick;

void RS485_Init(void);
void RS485_Reconfig(void);
void RS485_LoadConfig(void);
void RS485_SaveConfig(void);
void RS485_SaveBaud(u8 baud);
void RS485_ReconfigLater(void);
void RS485_Task(void);

#endif
