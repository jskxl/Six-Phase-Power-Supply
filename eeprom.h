#ifndef __EEPROM_H
#define __EEPROM_H
#include "stm32f10x.h"

#ifndef u8
#define u8 unsigned char
#endif
#ifndef u16
#define u16 unsigned int
#endif

/* 外接I2C EEPROM(AT24Cxx): SCL=PB12 SDA=PB13 */
#define EEPROM_DEV_ADDR 0xA0   /* 器件地址(写), A0/A1/A2接地时 */

void EEPROM_Init(void);
u8   EEPROM_Write(u16 addr, u8 *buf, u8 len);   /* 页写, 返回1=成功 */
u8   EEPROM_Read(u16 addr, u8 *buf, u8 len);    /* 随机读, 返回1=成功 */

#endif
