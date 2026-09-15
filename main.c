#include "sys.h"
#include "delay.h"
#include "key.h"
#include "lcd.h"
#include "ui.h"
#include "power_com.h"
#include "rs485.h"

//V1.0

#define RESTORE_GAP_MS 300u

static int32_t enc_last=0;
static int32_t enc_carry=0;
static u8  s_restore_on[CH_NUM];
static u16 s_restore_i[CH_NUM];

int main(void)
{
	u32 now;
	u32 bottom_t=0;
	u32 led_t=0;
	u8 last_mask=0xFF;
	u8 cfg_baud_old=0;
	u8 save_dirty=0;
	u32 save_tick=0;
	u32 last_op_tick=0;
	u32 last_auto_tick=0;
	u8 display_on=1;
	u8 restore_done=0;
	u8 restore_idx=0;
	u32 restore_tick=0;
	u32 cur_tick=0;

	delay_init();
	IWDG->KR = 0x5555;
	IWDG->PR = 4;
	IWDG->RLR = 1249;
	IWDG->KR = 0xAAAA;
	IWDG->KR = 0xCCCC;
	NVIC_Configuration();
	KEY_Init();
	TIM2_Tick_Init();
	Lcd_Init();
	UI_Init();
	PowerCom_Init();
	RS485_Init();
	{
		u8 i;
		for(i=0;i<CH_NUM;i++)
		{
			s_restore_on[i]=g_ch[i].on;
			s_restore_i[i]=g_ch[i].i_set;
		}
	}
	UI_DrawAll();
	UI_Draw485Led();
	enc_last=g_enc_pos;
	while(1)
	{
		s32 raw,det,inc;
		now=g_tick;
		IWDG->KR = 0xAAAA;
		PC_Task();
		RS485_Task();

		if(!restore_done && pc_meas.has_data)
		{
			restore_done=1;
			restore_idx=0;
			restore_tick=now;
		}
		if(restore_done && restore_idx < CH_NUM && (now-restore_tick) >= RESTORE_GAP_MS)
		{
			PW_SetChannelEx(restore_idx, s_restore_i[restore_idx], s_restore_on[restore_idx]);
			restore_idx++;
			restore_tick=now;
		}

		inc=g_enc_pos-enc_last;       /* 本轮新增量 */
		raw=inc+enc_carry;            /* 累计遗留余数(保持慢速转动精度) */
		det=raw/4;
		enc_carry=raw%4;
		enc_last=g_enc_pos;           /* 同步基准: 余数不残留在差值中 */

		if(inc!=0 || g_key.ch_short || g_key.ch_long || g_key.sel_short || g_key.sel_long ||
		   g_key.out_short || g_key.out_long || g_key.sw_short || g_key.sw_long)
		{
			last_op_tick=now;
			if(!display_on)
			{
				display_on=1;
				LCD_BLK_Set();
			}
		}

		if(display_on && (now-last_op_tick)>=120000)
		{
			display_on=0;
			LCD_BLK_Clr();
		}

		if(pc_meas.new_data)
		{
			u8 i;
			pc_meas.new_data=0;
			for(i=0;i<CH_NUM;i++)
			{
				g_ch[i].v_rb=pc_meas.out_v[i];
				g_ch[i].i_rb=pc_meas.out_i[i];
				UI_CurrentSample(i, pc_meas.out_i[i]);
				g_ch[i].on=(pc_meas.mask>>i)&1;
			}
			UI_DrawVin();
			if(pc_meas.mask!=last_mask){last_mask=pc_meas.mask;if(!g_cfg_mode)UI_DrawRowLabels();}
			if(!g_cfg_mode && now-bottom_t>=100)
			{
				bottom_t=now;
				UI_RefreshMainValues();
			}
		}

		if(!g_cfg_mode && (now-cur_tick) >= 500)
		{
			cur_tick=now;
			UI_CurrentTick();
		}

		if(g_cfg_mode)
		{
			g_key.ch_long=0; g_key.sel_long=0;
			g_key.out_long=0; g_key.sw_long=0;
			if(det)
			{
				if(g_cfg_sel==0)
				{
					s32 a=(s32)g_cfg_addr+det;
					if(a<1)a=1;
					if(a>31)a=31;
					g_cfg_addr=(u8)a;
				}
				else
				{
					s32 b=(s32)g_cfg_baud+det;
					if(b<0)b=0;
					if(b>5)b=5;
					g_cfg_baud=(u8)b;
				}
				UI_ConfigValues();
			}
			if(g_key.sw_short)
			{
				g_key.sw_short=0;
				g_cfg_sel^=1;
				UI_ConfigValues();
			}
			if(g_key.sel_short)
			{
				g_key.sel_short=0;
				rs485_addr=g_cfg_addr;
				rs485_baud_idx=g_cfg_baud;
				RS485_SaveConfig();
				if(g_cfg_baud != cfg_baud_old) RS485_ReconfigLater();
				g_cfg_mode=0;
				UI_DrawAll();
				UI_Draw485Led();
			}
			if(g_key.ch_short)
			{
				g_key.ch_short=0;
				g_cfg_mode=0;
				UI_DrawAll();
				UI_Draw485Led();
			}
			g_key.out_short=0;
			delay_ms(1);
			continue;
		}

		if(g_key.out_long)
		{
			g_key.out_long=0;
			if(!K2)
			{
				g_edit_mode=0;
				g_cfg_mode=1;
				cfg_baud_old=rs485_baud_idx;
				g_cfg_addr=rs485_addr;
				g_cfg_baud=rs485_baud_idx;
				g_cfg_sel=0;
				UI_DrawConfig();
			}
		}

		if(g_key.sel_long)
		{
			g_key.sel_long=0;
			if(g_edit_mode)
			{
				g_edit_mode=0;
				UI_DrawEditArea();
			}
			else
			{
				g_edit_mode=1;
				g_param=P_I;
				g_digit=EDIT_ONES;
				UI_DrawEditArea();
			}
		}

		if(g_key.ch_long)
		{
			g_key.ch_long=0;
		}

		if(g_key.sel_short)
		{
			g_key.sel_short=0;
		}

		if(g_key.ch_short)
		{
			u8 ch;
			g_key.ch_short=0;
			ch=g_page*3+g_sel;
			g_ch[ch].on=!g_ch[ch].on;
			PW_SetChannel(ch);
			UI_DrawRowLabels();
			UI_DrawEditArea();
			RS485_SaveConfig();
		}

		if(g_key.out_short)
		{
			u8 ch;
			g_key.out_short=0;
			ch=(u8)((g_page*3+g_sel+1)%6);
			g_page=(u8)(ch/3);
			g_sel=(u8)(ch%3);
			g_digit=EDIT_ONES;
			UI_DrawTop();
			UI_MainRedraw();
			UI_DrawEditArea();
		}

		if(g_key.sw_short)
		{
			g_key.sw_short=0;
			if(g_edit_mode)
			{
				g_digit^=1;
				UI_RedrawValue();
			}
		}
		if(g_key.sw_long)g_key.sw_long=0;
		g_key.ch_long=0;

		if(det)
		{
			if(g_edit_mode)
			{
				u8 ch=g_page*3+g_sel;
				s32 w;
				s32 nv;
				if(g_digit==EDIT_ONES)
					w=100;
				else
					w=1;
				nv=(s32)g_ch[ch].i_set+(s32)det*w;
				if(nv<0)nv=0;
				if(nv>1500)nv=1500;
				if(nv!=(s32)g_ch[ch].i_set)
				{
					g_ch[ch].i_set=(u16)nv;
					PW_SetCurr(ch);
					UI_RedrawValue();
					UI_DrawChannelValues(g_sel);
					save_dirty=1;
					save_tick=now;
				}
			}
		}

		if(now-led_t>=100){led_t=now;UI_Draw485Led();}
		if(display_on && !g_edit_mode && (now-last_op_tick)>=30000 && (now-last_auto_tick)>=3000)
		{
			g_page^=1;
			g_digit=EDIT_ONES;
			UI_MainRedraw();
			UI_DrawEditArea();
			last_auto_tick=now;
		}
		if(save_dirty && (now-save_tick)>=2000)
		{
			save_dirty=0;
			RS485_SaveConfig();
		}
		delay_ms(1);
	}
}
