#include "rs485.h"
#include "key.h"
#include "ui.h"
#include "power_com.h"
#include "eeprom.h"
#include "delay.h"

const u32 rs485_bauds[6] = {4800, 9600, 19200, 38400, 57600, 115200};

u8 rs485_addr = RS485_DEF_ADDR;
volatile u32 rs485_rx_tick = 0;
u8 rs485_baud_idx = RS485_DEF_BAUD_IDX;

#define CFG_MAGIC      0x4E4350

#define RX_BUF_SIZE 80
static u8  rx_buf[RX_BUF_SIZE];
static u8  rx_cnt = 0;
static u32 rx_last_tick = 0;
static u16 rx_gap_ms = 5;

#define REG_RO_END     0x000C
#define REG_MAX_ADDR   0x001E

static volatile u8  save_pending = 0;
static volatile u32 save_tick = 0;

static u16 crc16_modbus(u8 *buf, u8 len)
{
	u16 crc = 0xFFFF;
	u8 i;
	while(len--)
	{
		crc ^= *buf++;
		for(i=0;i<8;i++)
		{
			if(crc&1) crc=(crc>>1)^0xA001;
			else crc>>=1;
		}
	}
	return crc;
}

static void rs485_send(u8 *buf, u8 len)
{
	u8 i;
	u16 guard;
	GPIO_SetBits(GPIOA, GPIO_Pin_8);
	delay_us(20);
	for(i=0;i<len;i++)
	{
		guard=0;
		while((USART1->SR & USART_FLAG_TXE) == 0)
		{
			if(++guard > 500000) break;
		}
		USART1->DR = buf[i];
	}
	guard=0;
	while((USART1->SR & USART_FLAG_TC) == 0)
	{
		if(++guard > 500000) break;
	}
	GPIO_ResetBits(GPIOA, GPIO_Pin_8);
}

static void RS485_SendException(u8 addr, u8 func, u8 exCode)
{
	u16 crc;
	u8 buf[5];
	buf[0]=addr;
	buf[1]=(u8)(func|0x80);
	buf[2]=exCode;
	crc=crc16_modbus(buf,3);
	buf[3]=(u8)crc;
	buf[4]=(u8)(crc>>8);
	rs485_send(buf,5);
}

static u16 reg_read(u16 reg)
{
	if(reg == 0x0000) return pc_meas.in_v;
	if(reg >= 0x0001 && reg <= 0x0006) return pc_meas.out_v[reg-0x0001];
	if(reg >= 0x0007 && reg <= 0x000C) return pc_meas.out_i[reg-0x0007];
	if(reg >= 0x000D && reg <= 0x0012) return 2400;
	if(reg >= 0x0013 && reg <= 0x0018) return g_ch[reg-0x0013].i_set;
	if(reg >= 0x0019 && reg <= 0x001E) return g_ch[reg-0x0019].on;
	if(reg == 0x001F) return 0;
	if(reg == 0x0020) return rs485_baud_idx;
	return 0xFFFF;
}

static u8 reg_validate(u16 reg, u16 val, u8 *exCode)
{
	if(reg <= REG_RO_END) { *exCode=0x02; return 0; }
	if(reg >= 0x000D && reg <= 0x0018) return 1;
	if(reg >= 0x0019 && reg <= 0x001E) { if(val > 1){ *exCode=0x03; return 0; } return 1; }
	if(reg == 0x001F) { if(val > 1){ *exCode=0x03; return 0; } return 1; }
	if(reg == 0x0020) { if(val > 5){ *exCode=0x03; return 0; } return 1; }
	*exCode=0x02;
	return 0;
}

static u8 reg_write(u16 reg, u16 val, u8 *exCode)
{
	u8 i;
	if(reg <= REG_RO_END) { *exCode=0x02; return 0; }
	if(reg >= 0x000D && reg <= 0x0012)
	{
		return 1;
	}
	if(reg >= 0x0013 && reg <= 0x0018)
	{
		i = (u8)(reg-0x0013);
		if(val > 1500) val = 1500;
		g_ch[i].i_set = val;
		PW_SetCurr(i);
		if(!g_cfg_mode) UI_DrawEditArea();
		save_pending=1; save_tick=g_tick;
		return 1;
	}
	if(reg >= 0x0019 && reg <= 0x001E)
	{
		if(val > 1) { *exCode=0x03; return 0; }
		i = (u8)(reg-0x0019);
		g_ch[i].on = val ? 1 : 0;
		PW_SetChannel(i);
		if(!g_cfg_mode)
		{ UI_DrawRowLabels(); UI_DrawEditArea(); }
		save_pending=1; save_tick=g_tick;
		return 1;
	}
	if(reg == 0x001F)
	{
		if(val > 1){ *exCode=0x03; return 0; }
		if(val == 1) PW_CalZero(0);
		return 1;
	}
	if(reg == 0x0020)
	{
		if(val > 5){ *exCode=0x03; return 0; }
		RS485_SaveBaud((u8)val);
		rs485_baud_idx=(u8)val;
		RS485_ReconfigLater();
		return 1;
	}
	*exCode=0x02;
	return 0;
}

static void process_frame(void)
{
	u8 i, k;
	u8 addr, func, ex;
	u16 crc, crc_got;
	u8 resp[80];
	u8 rlen;

	if(rx_cnt < 8) { rx_cnt = 0; return; }
	addr = rx_buf[0];
	func = rx_buf[1];
	crc = crc16_modbus(rx_buf, (u8)(rx_cnt-2));
	crc_got = (u16)(rx_buf[rx_cnt-2] | (rx_buf[rx_cnt-1]<<8));
	if(crc != crc_got) return;

	if(addr == 0)
	{
		if(func == 0x06 && rx_cnt == 8 &&
		   rx_buf[2]==0x00 && rx_buf[3]==0x00)
		{
			u16 new_addr=(u16)((rx_buf[4]<<8)|rx_buf[5]);
			if(new_addr>=1 && new_addr<=31)
			{
				rs485_addr=(u8)new_addr;
				RS485_SaveConfig();
			}
			rs485_send(rx_buf, rx_cnt);
		}
		return;
	}
	if(addr != rs485_addr) return;

	if(func == 0x03)
	{
		u16 start = (u16)((rx_buf[2]<<8)|rx_buf[3]);
		u16 count = (u16)((rx_buf[4]<<8)|rx_buf[5]);
		if(rx_cnt != 8 || count == 0)
		{
			RS485_SendException(addr, 0x03, 0x03);
			return;
		}
		if(count > 33 || (u32)start+count > 33)
		{
			RS485_SendException(addr, 0x03, 0x02);
			return;
		}
		resp[0] = addr;
		resp[1] = 0x03;
		resp[2] = (u8)(count*2);
		for(i=0;i<count;i++)
		{
			u16 v = reg_read((u16)(start+i));
			resp[3+i*2]   = (u8)(v>>8);
			resp[4+i*2]   = (u8)v;
		}
		rlen = (u8)(3+count*2);
		crc = crc16_modbus(resp, rlen);
		resp[rlen++] = (u8)crc;
		resp[rlen++] = (u8)(crc>>8);
		rs485_send(resp, rlen);
		return;
	}

	if(func == 0x06)
	{
		u16 reg = (u16)((rx_buf[2]<<8)|rx_buf[3]);
		u16 val = (u16)((rx_buf[4]<<8)|rx_buf[5]);
		if(rx_cnt != 8) { RS485_SendException(addr, 0x06, 0x03); return; }
		if(reg_write(reg, val, &ex))
		{
			rs485_send(rx_buf, rx_cnt);
		}
		else
		{
			RS485_SendException(addr, 0x06, ex);
		}
		return;
	}

	if(func == 0x10)
	{
		u16 start = (u16)((rx_buf[2]<<8)|rx_buf[3]);
		u16 count = (u16)((rx_buf[4]<<8)|rx_buf[5]);
		u8 bcnt = rx_buf[6];
		if(rx_cnt != (u8)(9+count*2) || count == 0 || bcnt != count*2)
		{
			RS485_SendException(addr, 0x10, 0x03);
			return;
		}
		if((u32)start+count > 33)
		{
			RS485_SendException(addr, 0x10, 0x02);
			return;
		}
		for(k=0;k<count;k++)
		{
			u16 reg = (u16)(start+k);
			u16 val = (u16)((rx_buf[7+k*2]<<8)|rx_buf[8+k*2]);
			if(!reg_validate(reg, val, &ex))
			{
				RS485_SendException(addr, 0x10, ex);
				return;
			}
		}

		for(k=0;k<count;k++)
		{
			u16 reg = (u16)(start+k);
			u16 val = (u16)((rx_buf[7+k*2]<<8)|rx_buf[8+k*2]);
			if(!reg_write(reg, val, &ex))
			{
				RS485_SendException(addr, 0x10, ex);
				return;
			}
		}
		resp[0]=addr; resp[1]=0x10;
		resp[2]=rx_buf[2]; resp[3]=rx_buf[3];
		resp[4]=rx_buf[4]; resp[5]=rx_buf[5];
		crc = crc16_modbus(resp, 6);
		resp[6]=(u8)crc; resp[7]=(u8)(crc>>8);
		rs485_send(resp, 8);
		return;
	}

	RS485_SendException(addr, func, 0x01);
}

#define CFG_EE_ADDR 0x0000
#define CFG_SIZE    36

static u8 cfg_crc(u8 *buf)
{
	u8 s = 0;
	u8 i;
	for(i=0;i<35;i++) s = (u8)(s + buf[i]);
	return s;
}

static void cfg_pack(u8 *buf)
{
	u8 i;
	buf[0]=(u8)(CFG_MAGIC);
	buf[1]=(u8)(CFG_MAGIC>>8);
	buf[2]=(u8)(CFG_MAGIC>>16);
	buf[3]=rs485_addr;
	buf[4]=rs485_baud_idx;
	for(i=0;i<6;i++) buf[5+i]=g_ch[i].on;
	for(i=0;i<6;i++)
	{
		buf[11+i*2]=(u8)(g_ch[i].v_set);
		buf[12+i*2]=(u8)(g_ch[i].v_set>>8);
	}
	for(i=0;i<6;i++)
	{
		buf[23+i*2]=(u8)(g_ch[i].i_set);
		buf[24+i*2]=(u8)(g_ch[i].i_set>>8);
	}
	buf[35]=cfg_crc(buf);
}

void RS485_SaveConfig(void)
{
	u8 buf[CFG_SIZE];
	cfg_pack(buf);
	EEPROM_Write(CFG_EE_ADDR, buf, CFG_SIZE);
}

void RS485_SaveBaud(u8 baud)
{
	u8 b=rs485_baud_idx;
	rs485_baud_idx=baud;
	RS485_SaveConfig();
	rs485_baud_idx=b;
}

void RS485_LoadConfig(void)
{
	u8 buf[CFG_SIZE];
	u8 i;
	if(EEPROM_Read(CFG_EE_ADDR, buf, CFG_SIZE))
	{
		if(buf[0]==(u8)CFG_MAGIC && buf[1]==(u8)(CFG_MAGIC>>8) && buf[2]==(u8)(CFG_MAGIC>>16) && buf[35]==cfg_crc(buf))
		{
			rs485_addr=(buf[3]>=1 && buf[3]<=31)?buf[3]:RS485_DEF_ADDR;
			rs485_baud_idx=(buf[4]<=5)?buf[4]:RS485_DEF_BAUD_IDX;
			for(i=0;i<6;i++) g_ch[i].on=buf[5+i];
			for(i=0;i<6;i++) g_ch[i].v_set=(u16)(buf[11+i*2] | ((u16)buf[12+i*2]<<8));
			for(i=0;i<6;i++) g_ch[i].i_set=(u16)(buf[23+i*2] | ((u16)buf[24+i*2]<<8));
			return;
		}
	}
	rs485_addr=RS485_DEF_ADDR;
	rs485_baud_idx=RS485_DEF_BAUD_IDX;
}

static void rs485_uart_init(u32 baud)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	u16 gap;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_AFIO|RCC_APB2Periph_USART1, ENABLE);

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOA, GPIO_Pin_8);

	USART_InitStructure.USART_BaudRate = baud;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx|USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART1, ENABLE);

	gap = (u16)((38500UL + baud - 1) / baud);
	if(gap < 1) gap = 1;
	rx_gap_ms = gap;
}

void RS485_Init(void)
{
	EEPROM_Init();
	if(rs485_baud_idx > 5) rs485_baud_idx = RS485_DEF_BAUD_IDX;
	RS485_LoadConfig();
	rs485_uart_init(rs485_bauds[(rs485_baud_idx<=5)?rs485_baud_idx:RS485_DEF_BAUD_IDX]);
	rx_cnt = 0;
}

void RS485_Reconfig(void)
{
	rs485_uart_init(rs485_bauds[(rs485_baud_idx<=5)?rs485_baud_idx:RS485_DEF_BAUD_IDX]);
	rx_cnt = 0;
}

static volatile u8  reconfig_pending = 0;
static volatile u32 reconfig_at = 0;

void RS485_ReconfigLater(void)
{
	reconfig_at = g_tick + 100;
	reconfig_pending = 1;
}
void RS485_Task(void)
{
	if(reconfig_pending && (g_tick >= reconfig_at))
	{
		reconfig_pending = 0;
		RS485_Reconfig();
	}
	if(save_pending && (g_tick - save_tick) >= 2000)
	{
		save_pending = 0;
		RS485_SaveConfig();
	}
	if(rx_cnt > 0 && (g_tick - rx_last_tick) >= rx_gap_ms)
	{
		process_frame();
		rx_cnt = 0;
	}
}

void USART1_IRQHandler(void)
{
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		u8 b = (u8)(USART1->DR & 0xFF);
		if(rx_cnt < RX_BUF_SIZE) rx_buf[rx_cnt++] = b;
		rs485_rx_tick = g_tick;
		rx_last_tick = g_tick;
	}
	if(USART_GetITStatus(USART1, USART_IT_ORE) != RESET)
	{
		(void)(USART1->DR);
	}
}
