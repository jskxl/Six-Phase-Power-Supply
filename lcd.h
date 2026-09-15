#ifndef __LCD_H
#define __LCD_H
#include "sys.h"
#include "stdlib.h"

#define LCD_W 240
#define LCD_H 240

#define u8  unsigned char
#define u16 unsigned int
#define u32 unsigned long

#define LCD_SCLK_Clr()  GPIO_ResetBits(GPIOA,GPIO_Pin_5)
#define LCD_SCLK_Set()  GPIO_SetBits(GPIOA,GPIO_Pin_5)
#define LCD_SDIN_Clr()  GPIO_ResetBits(GPIOA,GPIO_Pin_7)
#define LCD_SDIN_Set()  GPIO_SetBits(GPIOA,GPIO_Pin_7)
#define LCD_RST_Clr()   GPIO_ResetBits(GPIOA,GPIO_Pin_4)
#define LCD_RST_Set()   GPIO_SetBits(GPIOA,GPIO_Pin_4)
#define LCD_DC_Clr()    GPIO_ResetBits(GPIOB,GPIO_Pin_0)
#define LCD_DC_Set()    GPIO_SetBits(GPIOB,GPIO_Pin_0)
#define LCD_BLK_Clr()   GPIO_ResetBits(GPIOB,GPIO_Pin_1)
#define LCD_BLK_Set()   GPIO_SetBits(GPIOB,GPIO_Pin_1)

#define LCD_CMD  0
#define LCD_DATA 1

extern u16 BACK_COLOR, POINT_COLOR;

void Lcd_Init(void);
void LCD_Clear(u16 Color);
void Address_set(u16 x1,u16 y1,u16 x2,u16 y2);
void LCD_WR_DATA8(u8 da);
void LCD_WR_DATA(u16 da);
void LCD_WR_REG(u8 da);
void LCD_DrawPoint(u16 x,u16 y);
void LCD_Fill(u16 xsta,u16 ysta,u16 xend,u16 yend,u16 color);
void LCD_DrawLine(u16 x1,u16 y1,u16 x2,u16 y2);
void LCD_DrawRectangle(u16 x1,u16 y1,u16 x2,u16 y2);
void Draw_Circle(u16 x0,u16 y0,u8 r);
void LCD_ShowChar(u16 x,u16 y,u8 num,u8 mode);
void LCD_ShowString(u16 x,u16 y,const u8 *p);
void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len);   /* 数字, 高位为0显示空格 */
void LCD_ShowNumCenti(u16 x,u16 y,u16 centi,u8 mode); /* 0.01??λ, ??????????, С???????λ */
void LCD_ShowHanzi16(u16 x,u16 y,u8 idx);             /* 16x16 ???? */
void LCD_ShowHanzi12(u16 x,u16 y,u8 idx);             /* 12x12 ???? */
void LCD_ShowAscii68(u16 x,u16 y,const char *s);      /* 6x8 ASCII */
void LCD_ShowBig(u16 x,u16 y,u8 idx);                 /* 7段大数字 14x28 */
/* 大数字字符索引: 0~9, 小数点, V, A */
#define BIG_0   0
#define BIG_1   1
#define BIG_2   2
#define BIG_3   3
#define BIG_4   4
#define BIG_5   5
#define BIG_6   6
#define BIG_7   7
#define BIG_8   8
#define BIG_9   9
#define BIG_DOT 10
#define BIG_V   11
#define BIG_A   12

/* ??? */
#define WHITE       0xFFFF
#define BLACK       0x0000
#define BLUE        0x001F
#define RED         0xF800
#define MAGENTA     0xF81F
#define GREEN       0x07E0
#define CYAN        0x07FF
#define YELLOW      0xFFE0
#define ORANGE      0xFD20
#define LGRAY       0xC618
#define DARKGRAY    0x4208

#endif
