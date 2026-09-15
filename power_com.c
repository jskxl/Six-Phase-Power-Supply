#include "power_com.h"
#include "key.h"
#include "ui.h"
PC_MEAS_T pc_meas;

static u8 rx_state = 0;
static u8 rx_func = 0;
static u8 rx_len = 0;
static u8 rx_data[28];
static u8 rx_cnt = 0;
static u8 rx_crc_l = 0;

static u8  cmd_buf[25];
static u8  cmd_len = 0;
static u8  cmd_func = 0;
static u8  cmd_retry = 0;
static u32 cmd_tick = 0;

static u16 crc16(u8 *buf, u8 len)
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

static u8 get_len(u8 func)
{
	if(func==PC_FUNC_SET_CH)   return 6;
	if(func==PC_FUNC_SET_MASK) return 1;
	if(func==PC_FUNC_SET_VOLT) return 3;
	if(func==PC_FUNC_SET_CURR) return 3;
	if(func==PC_FUNC_QUERY)    return 0;
	if(func==PC_FUNC_SET_ALL)  return 24;
	if(func==PC_FUNC_CAL_ZERO) return 1;
	if(func==PC_FUNC_MEASURE)  return 28;
	if(func==PC_FUNC_ACK)      return 2;
	if(func==PC_FUNC_ALARM)    return 2;
	return 0xFF;
}

static void uart3_send_byte(u8 b)
{
	u16 guard = 0;
	while((USART3->SR & USART_FLAG_TXE) == 0)
	{
		if(++guard > 100000) break;
	}
	USART3->DR = b;
}

static void pc_send_frame(u8 func, const u8 *data, u8 len)
{
	u8 buf[32];
	u8 i;
	u16 crc;
	buf[0]=PC_SYNC0;
	buf[1]=PC_SYNC1;
	buf[2]=func;
	for(i=0;i<len;i++) buf[3+i]=data[i];
	crc=crc16(buf+2, (u8)(1+len));
	buf[3+len]=(u8)crc;
	buf[4+len]=(u8)(crc>>8);
	for(i=0;i<(u8)(len+5);i++) uart3_send_byte(buf[i]);
}

void PC_SendCmd(u8 func, const u8 *data, u8 len)
{
	u8 i;
	cmd_buf[0]=func;
	for(i=0;i<len;i++) cmd_buf[1+i]=data[i];
	cmd_len=len;
	cmd_func=func;
	cmd_retry=1;
	cmd_tick=g_tick;
	pc_meas.ack_flag=0;
	pc_meas.cmd_fail=0;
	pc_meas.cmd_busy=1;
	pc_send_frame(func, data, len);
}

static void on_frame(u8 func, u8 *data);

static void rx_byte(u8 b)
{
	switch(rx_state)
	{
		case 0:
			if(b == PC_SYNC0) rx_state = 1;
			break;
		case 1:
			if(b == PC_SYNC1) rx_state = 2;
			else if(b == PC_SYNC0) rx_state = 1;
			else rx_state = 0;
			break;
		case 2:
			rx_func = b;
			rx_len = get_len(b);
			if(rx_len == 0xFF) rx_state = 0;
			else if(rx_len == 0) rx_state = 4;
			else { rx_cnt = 0; rx_state = 3; }
			break;
		case 3:
			if(rx_cnt < 28) rx_data[rx_cnt++] = b;
			if(rx_cnt == rx_len) rx_state = 4;
			break;
		case 4:
			rx_crc_l = b;
			rx_state = 5;
			break;
		case 5:
			{
				u8 tb[29];
				u8 i;
				u16 crc;
				tb[0] = rx_func;
				for(i=0;i<rx_len;i++) tb[1+i] = rx_data[i];
				crc = crc16(tb, (u8)(1+rx_len));
				if(((u8)crc) == rx_crc_l && ((u8)(crc>>8)) == b)
				{
					on_frame(rx_func, rx_data);
				}
			}
			rx_state = 0;
			break;
		default:
			rx_state = 0;
			break;
	}
}

static void on_frame(u8 func, u8 *data)
{
	u8 i;
	switch(func)
	{
		case PC_FUNC_MEASURE:
			if(pc_meas.has_data && (u8)(pc_meas.seq+1) != data[0])
			{
				pc_meas.seq_miss++;
			}
			pc_meas.seq=data[0];
			pc_meas.in_v=(u16)((data[1]<<8)|data[2]);
			for(i=0;i<6;i++)
			{
				pc_meas.out_v[i]=(u16)((data[3+i*2]<<8)|data[4+i*2]);
				pc_meas.out_i[i]=(u16)((data[15+i*2]<<8)|data[16+i*2]);
			}
			pc_meas.mask=data[27];
			pc_meas.last_tick=g_tick;
			pc_meas.has_data=1;
			pc_meas.lost=0;
			pc_meas.new_data=1;
			break;

		case PC_FUNC_ACK:
			pc_meas.ack_func=data[0];
			pc_meas.ack_status=data[1];
			pc_meas.ack_flag=1;
			break;

		case PC_FUNC_ALARM:
			pc_meas.alarm=data[0];
			pc_meas.fault=data[1];
			pc_meas.alarm_new=1;
			break;

		default:
			break;
	}
}


void PW_SetChannelEx(u8 ch, u16 i_set, u8 on)
{
	u8 d[6];
	d[0]=ch+1;
	d[1]=0;
	d[2]=0;
	d[3]=(u8)(i_set>>8);
	d[4]=(u8)i_set;
	d[5]=on;
	PC_SendCmd(PC_FUNC_SET_CH,d,6);
}

void PW_SetChannel(u8 ch)
{
	PW_SetChannelEx(ch, g_ch[ch].i_set, g_ch[ch].on);
}

void PW_SetVolt(u8 ch)
{
	(void)ch;
}

void PW_SetCurr(u8 ch)
{
	u8 d[3];
	d[0]=ch+1;
	d[1]=(u8)(g_ch[ch].i_set>>8);
	d[2]=(u8)(g_ch[ch].i_set);
	PC_SendCmd(PC_FUNC_SET_CURR,d,3);
}

void PW_CalZero(u8 ch)
{
	u8 d[1];
	d[0]=ch;
	PC_SendCmd(PC_FUNC_CAL_ZERO,d,1);
}

void PowerCom_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB|RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx|USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART3, ENABLE);

	pc_meas.lost = 1;
	pc_meas.query_tick = 0;
}

void PC_Task(void)
{
	if(pc_meas.cmd_busy && pc_meas.ack_flag)
	{
		if(pc_meas.ack_func == cmd_func)
		{
			pc_meas.cmd_busy = 0;
			pc_meas.ack_flag = 0;
		}
	}

	if(pc_meas.cmd_busy && (g_tick-cmd_tick) >= 20)
	{
		if(cmd_retry < 3)
		{
			cmd_retry++;
			cmd_tick = g_tick;
			pc_send_frame(cmd_func, cmd_buf+1, cmd_len);
		}
		else
		{
			pc_meas.cmd_busy = 0;
			pc_meas.cmd_fail = 1;
		}
	}

	if(pc_meas.has_data && (g_tick-pc_meas.last_tick) >= 200)
	{
		pc_meas.lost = 1;
	}

	if(pc_meas.lost && (g_tick-pc_meas.query_tick) >= 500)
	{
		pc_meas.query_tick = g_tick;
		pc_send_frame(PC_FUNC_QUERY, 0, 0);
	}
}

void USART3_IRQHandler(void)
{
	u8 b;
	if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
	{
		b = (u8)(USART3->DR & 0xFF);
		rx_byte(b);
	}
	if(USART_GetITStatus(USART3, USART_IT_ORE) != RESET)
	{
		(void)(USART3->DR);
	}
}
