#include "ui.h"
#include "power_com.h"
#include "rs485.h"
#include "key.h"

CH_T g_ch[CH_NUM];
u8 g_page=0;
u8 g_sel=0;
u8 g_param=P_V;
u8 g_digit=EDIT_ONES;
u8 g_edit_mode=0;
u8 g_cfg_mode=0;
u8 g_cfg_sel=0;
u8 g_cfg_addr=1;
u8 g_cfg_baud=1;
u32 g_rx_cnt=0;

#if 0
static void draw_num68(u16 x,u16 y,u16 centi)
{
	u8 buf[7];
	u8 i,n=0,t;
	u16 ip=centi/100;
	u16 fp=centi%100;
	while(1)
	{
		buf[n++]=(u8)('0'+ip%10);
		ip/=10;
		if(ip==0||n>=5)break;
	}
	for(i=0;i<n/2;i++){t=buf[i];buf[i]=buf[n-1-i];buf[n-1-i]=t;}
	buf[n++]='.';
	buf[n++]=(u8)('0'+fp/10);
	buf[n++]=(u8)('0'+fp%10);
	buf[n]=0;
	LCD_ShowAscii68(x,y,(const char*)buf);
}
#endif

static void draw_big_val(u16 x,u16 y,u16 centi,u8 hl,u16 color)
{
	u8 d[4];
	u8 i;
	u16 ip=centi/100;
	u16 fp=centi%100;
	d[0]=(u8)(ip/10);
	d[1]=(u8)(ip%10);
	d[2]=(u8)(fp/10);
	d[3]=(u8)(fp%10);
	for(i=0;i<4;i++)
	{
		u8 hl_this=0;
		if(hl!=0xFF)
		{
			if(hl==EDIT_ONES && i==1)hl_this=1;
			if(hl==EDIT_FINE && i==3)hl_this=1;
		}
		if(hl_this)
		{
			LCD_Fill(x+i*22-1,y-2,x+i*22+15,y+29,ORANGE);
			BACK_COLOR=ORANGE;
			POINT_COLOR=BLACK;
		}
		else
		{
			BACK_COLOR=BLACK;
			POINT_COLOR=color;
		}
		LCD_ShowBig(x+i*22,y,d[i]);
	}
	LCD_Fill(x+38,y+22,x+41,y+27,color);
}

static void draw_big_cur(u16 x,u16 y,u16 milli,u8 hl,u16 color)
{
	u8 d[4];
	u8 i;
	u16 ip=milli/1000;
	u16 fp=milli%1000;
	d[0]=(u8)ip;
	d[1]=(u8)(fp/100);
	d[2]=(u8)((fp/10)%10);
	d[3]=(u8)(fp%10);
	for(i=0;i<4;i++)
	{
		u8 hl_this=0;
		if(hl!=0xFF)
		{
			if(hl==EDIT_ONES && i==1)hl_this=1;
			if(hl==EDIT_FINE && i==3)hl_this=1;
		}
		if(hl_this)
		{
			LCD_Fill(x+i*22-1,y-2,x+i*22+15,y+29,ORANGE);
			BACK_COLOR=ORANGE;
			POINT_COLOR=BLACK;
		}
		else
		{
			BACK_COLOR=BLACK;
			POINT_COLOR=color;
		}
		LCD_ShowBig(x+i*22,y,d[i]);
	}
	LCD_Fill(x+16,y+22,x+19,y+27,color);
}

#define CUR_BLK_NUM  4u
#define CUR_JUMP_MA  50u

static u32 s_i_sum[CH_NUM];
static u8  s_i_cnt[CH_NUM];
static u16 s_i_show[CH_NUM];
static u16 s_i_blk[CH_NUM][CUR_BLK_NUM];
static u8  s_i_blkn[CH_NUM];
static u16 last_mi[CH_NUM];

void UI_CurrentSample(u8 ch, u16 ma)
{
	s_i_sum[ch] += ma;
	s_i_cnt[ch]++;
}

void UI_CurrentTick(void)
{
	u8 ch;
	for(ch=0;ch<CH_NUM;ch++)
	{
		u16 avg;
		u16 show;
		u32 sum;
		u8 k;
		if(s_i_cnt[ch] == 0u) continue;
		avg = (u16)(s_i_sum[ch] / s_i_cnt[ch]);
		s_i_sum[ch] = 0u;
		s_i_cnt[ch] = 0u;
		for(k=(u8)(CUR_BLK_NUM-1u);k>0u;k--) s_i_blk[ch][k] = s_i_blk[ch][k-1u];
		s_i_blk[ch][0] = avg;
		if(s_i_blkn[ch] < CUR_BLK_NUM) s_i_blkn[ch]++;
		show = s_i_show[ch];
		if(((s32)avg > (s32)show + (s32)CUR_JUMP_MA) ||
		   ((s32)avg + (s32)CUR_JUMP_MA < (s32)show))
		{
			for(k=0u;k<CUR_BLK_NUM;k++) s_i_blk[ch][k] = avg;
			s_i_blkn[ch] = CUR_BLK_NUM;
			show = avg;
		}
		else
		{
			sum = 0u;
			for(k=0u;k<s_i_blkn[ch];k++) sum += s_i_blk[ch][k];
			show = (u16)(sum / s_i_blkn[ch]);
		}
		s_i_show[ch] = show;
		if(ch/3 != g_page) continue;
		if(show != last_mi[ch])
		{
			u8 r = (u8)(ch%3);
			u16 y0 = (u16)(22u + r*48u);
			if(r==2) y0 += 4u;
			LCD_Fill(137,y0+4,217,y0+36,BLACK);
			draw_big_cur(138,y0+6,show,0xFF,YELLOW);
			last_mi[ch] = show;
		}
	}
}

static void draw_row(u8 r)
{
	u16 y0=22+r*48;
	u8 ch=g_page*3+r;
	u8 sel=(r==g_sel);
	if(r==2)y0+=4;

	if(sel)
	{
		LCD_Fill(2,y0-2,22,y0+29,BLUE);
		BACK_COLOR=BLUE;
		POINT_COLOR=WHITE;
	}
	else
	{
		LCD_Fill(2,y0-2,22,y0+29,BLACK);
		BACK_COLOR=BLACK;
		POINT_COLOR=WHITE;
	}
	LCD_ShowBig(4,y0+0,ch+1);
	BACK_COLOR=BLACK;

	BACK_COLOR=BLACK;
	if(g_ch[ch].on)
	{
		LCD_Fill(2,y0+29,21,y0+44,GREEN);
		BACK_COLOR=GREEN;
		POINT_COLOR=BLACK;
	}
	else
	{
		LCD_Fill(2,y0+29,21,y0+44,DARKGRAY);
		BACK_COLOR=DARKGRAY;
		POINT_COLOR=WHITE;
	}
	LCD_ShowChar(4,y0+29,'E',0);
	LCD_ShowChar(12,y0+29,'N',0);
	BACK_COLOR=BLACK;

	LCD_Fill(27,y0+4,239,y0+36,BLACK);
	draw_big_val(28,y0+6,g_ch[ch].v_rb,0xFF,GREEN);
	BACK_COLOR=BLACK;
	POINT_COLOR=GREEN;
	LCD_ShowBig(110,y0+6,BIG_V);

	draw_big_cur(138,y0+6,s_i_show[ch],0xFF,YELLOW);
	BACK_COLOR=BLACK;
	POINT_COLOR=YELLOW;
	LCD_ShowBig(224,y0+6,BIG_A);
}

void UI_Init(void)
{
	u8 i;
	for(i=0;i<CH_NUM;i++)
	{
		g_ch[i].v_set=0;
		g_ch[i].i_set=0;
		g_ch[i].v_rb=0;
		g_ch[i].i_rb=0;
		g_ch[i].on=0;
	}
	g_page=0;
	g_sel=0;
	g_param=P_V;
	g_digit=EDIT_ONES;
}


static void cfg_big_num(u16 x,u16 y,u32 num,u8 sel)
{
	u8 buf[6];
	u8 n=0,i,tmp;
	u32 t=num;
	if(num==0)buf[n++]=0;
	else { while(t){ buf[n++]=(u8)(t%10); t/=10; } }
	for(i=0;i<n/2;i++){tmp=buf[i];buf[i]=buf[n-1-i];buf[n-1-i]=tmp;}
	if(sel)
	{
		LCD_Fill(x-3,y-3,x+(n-1)*16+16,y+31,ORANGE);
	}
	for(i=0;i<n;i++)
	{
		if(sel)
		{
			BACK_COLOR=ORANGE;
			POINT_COLOR=BLACK;
		}
		else
		{
			BACK_COLOR=BLACK;
			POINT_COLOR=WHITE;
		}
		LCD_ShowBig(x+i*16,y,buf[i]);
	}
	BACK_COLOR=BLACK;
}

static void cfg_draw_values(void)
{
	LCD_Fill(106,50,144,86,BLACK);
	cfg_big_num(110,54,g_cfg_addr,(g_cfg_sel==0));
	LCD_Fill(106,110,208,146,BLACK);
	cfg_big_num(110,114,rs485_bauds[g_cfg_baud],(g_cfg_sel==1));
}

void UI_DrawConfig(void)
{
	LCD_Clear(BLACK);

	BACK_COLOR=BLACK;
	POINT_COLOR=WHITE;
	LCD_ShowString(76,10,"RS485 CONFIG");
	LCD_Fill(20,30,220,30,WHITE);

	POINT_COLOR=YELLOW;
	LCD_ShowString(40,60,"ADDR:");
	LCD_ShowString(40,120,"BAUD:");

	POINT_COLOR=DARKGRAY;
	LCD_Fill(30,100,210,100,DARKGRAY);

	cfg_draw_values();
}

void UI_ConfigValues(void)
{
	cfg_draw_values();
}
void UI_DrawAll(void)
{
	LCD_Clear(BLACK);
	UI_DrawTop();
	UI_MainRedraw();
	UI_DrawEditArea();
}




void UI_Draw485Led(void)
{
	u8 on = (u8)((g_tick - rs485_rx_tick) < 300);
	LCD_Fill(230,2,238,9, on ? GREEN : BLACK);
}

static u16 last_vin=0xFFFF;

static void draw_top_vin(u16 x,u16 y)
{
	u8 ip=pc_meas.in_v/100;
	u8 fp=pc_meas.in_v%100;
	if(ip>99)ip=99;
	POINT_COLOR=WHITE;
	LCD_ShowChar(x,y,(u8)('0'+ip/10),0);
	LCD_ShowChar(x+8,y,(u8)('0'+ip%10),0);
	LCD_ShowChar(x+16,y,'.',0);
	LCD_ShowChar(x+24,y,(u8)('0'+fp/10),0);
	LCD_ShowChar(x+32,y,(u8)('0'+fp%10),0);
	LCD_ShowChar(x+40,y,'V',0);
}

void UI_DrawVin(void)
{
	if(g_cfg_mode) return;
	if(pc_meas.in_v==last_vin) return;
	LCD_Fill(40,0,87,15,BLACK);
	BACK_COLOR=BLACK;
	draw_top_vin(40,0);
	last_vin=pc_meas.in_v;
}


void UI_DrawTop(void)
{
	LCD_Fill(0,0,239,15,BLACK);
	BACK_COLOR=BLACK;
	POINT_COLOR=WHITE;
	POINT_COLOR=YELLOW;
	LCD_ShowChar(4,0,'V',0);
	LCD_ShowChar(12,0,'I',0);
	LCD_ShowChar(20,0,'N',0);
	LCD_ShowChar(28,0,':',0);
	POINT_COLOR=WHITE;
	draw_top_vin(40,0);

	POINT_COLOR=YELLOW;
	LCD_ShowChar(100,0,'A',0);
	LCD_ShowChar(108,0,'D',0);
	LCD_ShowChar(116,0,'D',0);
	LCD_ShowChar(124,0,'R',0);
	LCD_ShowChar(132,0,':',0);
	POINT_COLOR=WHITE;
	LCD_ShowChar(142,0,(u8)('0'+rs485_addr/10),0);
	LCD_ShowChar(150,0,(u8)('0'+rs485_addr%10),0);
	POINT_COLOR=WHITE;
	{
		u32 baud=rs485_bauds[(rs485_baud_idx<=5)?rs485_baud_idx:1];
		u16 bx=170;
		if(baud>=100000)LCD_ShowChar(bx,0,(u8)('0'+baud/100000),0);
		bx+=8;
		if(baud>=10000)LCD_ShowChar(bx,0,(u8)('0'+(baud/10000)%10),0);
		bx+=8;
		LCD_ShowChar(bx,0,(u8)('0'+(baud/1000)%10),0);
		bx+=8;
		LCD_ShowChar(bx,0,(u8)('0'+(baud/100)%10),0);
		bx+=8;
		LCD_ShowChar(bx,0,(u8)('0'+(baud/10)%10),0);
		bx+=8;
		LCD_ShowChar(bx,0,(u8)('0'+baud%10),0);
	}
	LCD_Fill(0,16,239,16,WHITE);
	last_vin=pc_meas.in_v;
}
void UI_MainRedraw(void)
{
	u8 r;
	LCD_Fill(0,17,239,168,BLACK);
	POINT_COLOR=GREEN;
	LCD_Fill(0,68,239,68,GREEN);
	LCD_Fill(0,116,239,116,GREEN);
	for(r=0;r<3;r++)draw_row(r);
}

void UI_DrawRowLabels(void)
{
	u8 r;
	for(r=0;r<3;r++)draw_row(r);
}

void UI_DrawChannelValues(u8 r)
{
	u16 y0=22+r*48;
	u8 ch=g_page*3+r;
	if(r==2)y0+=4;
	LCD_Fill(27,y0+4,239,y0+36,BLACK);
	draw_big_val(28,y0+6,g_ch[ch].v_rb,0xFF,GREEN);
	BACK_COLOR=BLACK;
	POINT_COLOR=GREEN;
	LCD_ShowBig(110,y0+6,BIG_V);
	draw_big_cur(138,y0+6,s_i_show[ch],0xFF,YELLOW);
	BACK_COLOR=BLACK;
	POINT_COLOR=YELLOW;
	LCD_ShowBig(224,y0+6,BIG_A);
}

static void draw_ch_char(u16 x,u16 y,u8 c,u8 hl,u16 color);

static void draw_ch_line(u16 x,u16 y,u8 ch,u8 sel)
{
	u16 i=g_ch[ch].i_set;
	u8 iip=(u8)(i/1000);
	u16 ifp=i%1000;
	u8 ed=(u8)(sel && g_edit_mode);
	u8 ei=(u8)(ed && g_param==P_I);
	u16 color = sel ? YELLOW : WHITE;

	POINT_COLOR = color;
	BACK_COLOR = BLACK;

	LCD_ShowChar(x,y,'C',0);
	LCD_ShowChar(x+8,y,'C',0);
	LCD_ShowChar(x+16,y,'1'+ch,0);
	LCD_ShowChar(x+24,y,':',0);

	LCD_ShowChar(x+40,y,(u8)('0'+iip),0);
	LCD_ShowChar(x+48,y,'.',0);
	draw_ch_char(x+56,y,(u8)('0'+(ifp/100)%10),(u8)(ei && g_digit==EDIT_ONES), color);
	LCD_ShowChar(x+64,y,(u8)('0'+(ifp/10)%10),0);
	draw_ch_char(x+72,y,(u8)('0'+ifp%10),(u8)(ei && g_digit==EDIT_FINE), color);
	LCD_ShowChar(x+80,y,'A',0);
}

void UI_RedrawValue(void)
{
	u8 ch=g_page*3+g_sel;
	u16 x=(g_page==0)?4:122;
	draw_ch_line(x,174+g_sel*22,ch,1);
}


static u16 last_mv[6];

void UI_RefreshMainValues(void)
{
	u8 ch;
	for(ch=0;ch<6;ch++)
	{
		u8 r=ch%3;
		u16 y0=22+r*48;
		if(r==2)y0+=4;
		if(ch/3 != g_page) continue;
		{
			s32 dv=(s32)g_ch[ch].v_rb-(s32)last_mv[ch];
			if(dv<0)dv=-dv;
			if(dv!=0)
			{
				LCD_Fill(27,y0+4,107,y0+36,BLACK);
				draw_big_val(28,y0+6,g_ch[ch].v_rb,0xFF,GREEN);
				last_mv[ch]=g_ch[ch].v_rb;
			}
		}

	}
}
static void draw_ch_char(u16 x,u16 y,u8 c,u8 hl,u16 color)
{
	if(hl)
	{
		LCD_Fill(x,y,x+7,y+15,ORANGE);
		BACK_COLOR=ORANGE;
		POINT_COLOR=BLACK;
		LCD_ShowChar(x,y,c,1);
		BACK_COLOR=BLACK;
		POINT_COLOR=color;
	}
	else
	{
		LCD_ShowChar(x,y,c,0);
	}
}


void UI_DrawEditArea(void)
{
	u8 r;
	LCD_Fill(0,171,239,239,BLACK);
	POINT_COLOR=GREEN;
	LCD_Fill(0,170,239,170,GREEN);
	for(r=0;r<3;r++)
	{
		draw_ch_line(4,  174+r*22, r,   (u8)(g_page==0 && r==g_sel));
		draw_ch_line(122,174+r*22, r+3, (u8)(g_page==1 && r==g_sel));
	}
}

