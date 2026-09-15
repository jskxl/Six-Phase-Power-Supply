#ifndef __UI_H
#define __UI_H
#include "lcd.h"

#define CH_NUM 6
#define CH_PER_PAGE 3

typedef struct
{
	u16 v_set;   /* voltage set 0.01V */
	u16 i_set;   /* current set 0.001A */
	u16 v_rb;    /* voltage readback 0.01V */
	u16 i_rb;    /* current readback 0.001A */
	u8  on;      /* output on/off */
} CH_T;

#define P_V 0
#define P_I 1

#define EDIT_ONES  0   /* edit digit: ones (V:1V / I:1A) */
#define EDIT_FINE  1   /* edit digit: least (V:0.01V / I:0.001A) */

extern CH_T g_ch[CH_NUM];
extern u8 g_page;       /* 0=CH1-3, 1=CH4-6 */
extern u8 g_sel;        /* 0..2 selected channel in page */
extern u8 g_param;      /* P_V/P_I */
extern u8 g_digit;      /* EDIT_ONES/EDIT_FINE */
extern u8 g_edit_mode;  /* 1=bottom edit mode */
extern u8 g_cfg_mode;   /* 1=RS485 config page */
extern u8 g_cfg_sel;    /* 0=addr 1=baud */
extern u8 g_cfg_addr;   /* 配置界面地址暂存(旋转预览, 中键确认生效) */
extern u8 g_cfg_baud;   /* 配置界面波特率暂存 */
extern u32 g_rx_cnt;    /* received valid frame count */

void UI_Init(void);
void UI_DrawAll(void);
void UI_DrawTop(void);
void UI_DrawVin(void);
void UI_DrawRowLabels(void);
void UI_DrawChannelValues(u8 r);
void UI_MainRedraw(void);
void UI_RedrawValue(void);
void UI_DrawEditArea(void);
void UI_DrawConfig(void);
void UI_Draw485Led(void);
void UI_ConfigValues(void);
void UI_RefreshMainValues(void);
void UI_CurrentSample(u8 ch, u16 ma);
void UI_CurrentTick(void);

#endif
