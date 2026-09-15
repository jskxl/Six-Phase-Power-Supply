#ifndef __KEY_H
#define __KEY_H
#include "stm32f10x.h"

/* 按键/编码器引脚(旧板): 按下为低电平 */
#define K1    GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_9)   /* CH  键 */
#define K2    GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_8)   /* SEL 键 */
#define K3    GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_7)   /* OUT 键 */
#define EN_A  GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_6)
#define EN_B  GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_5)
#define EN_SW GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_4)

typedef struct
{
	volatile uint8_t ch_short;
	volatile uint8_t ch_long;
	volatile uint8_t sel_short;
	volatile uint8_t sel_long;
	volatile uint8_t out_short;
	volatile uint8_t out_long;
	volatile uint8_t sw_short;
	volatile uint8_t sw_long;
} KEY_EVT;

extern volatile uint32_t g_tick;    /* 1ms 时基 */
extern volatile int32_t  g_enc_pos; /* 编码器累计位置(软件解码) */
extern volatile KEY_EVT  g_key;

void KEY_Init(void);       /* 按键+编码器GPIO初始化(关闭JTAG,保留SWD) */
void TIM2_Tick_Init(void); /* 1ms 时基: 按键扫描 + 编码器解码 */

#endif
