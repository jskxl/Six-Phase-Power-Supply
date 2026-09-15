#include "key.h"

volatile uint32_t g_tick = 0;
volatile int32_t  g_enc_pos = 0;
volatile KEY_EVT  g_key;

static uint8_t  kb[4] = {0,0,0,0};
static uint8_t  kc[4] = {0,0,0,0};
static uint32_t kpress[4] = {0,0,0,0};
static uint8_t  kheld[4] = {0,0,0,0};
static uint8_t  klong[4] = {0,0,0,0};
static uint8_t  enc_last = 0;

static const uint16_t long_t[4] = {1500,1500,1500,2000};

static void ev_short(uint8_t i)
{
	switch(i)
	{
		case 0: g_key.ch_short=1;  break;
		case 1: g_key.sel_short=1; break;
		case 2: g_key.out_short=1; break;
		default: g_key.sw_short=1; break;
	}
}

static void ev_long(uint8_t i)
{
	switch(i)
	{
		case 0: g_key.ch_long=1;  break;
		case 1: g_key.sel_long=1; break;
		case 2: g_key.out_long=1; break;
		default: g_key.sw_long=1; break;
	}
}

void KEY_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB|RCC_APB2Periph_AFIO, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void TIM2_Tick_Init(void)
{
	TIM_TimeBaseInitTypeDef TIM_Init;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	TIM_Init.TIM_Prescaler = 72-1;
	TIM_Init.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_Init.TIM_Period = 1000-1;
	TIM_Init.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInit(TIM2, &TIM_Init);

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	TIM_ClearFlag(TIM2, TIM_FLAG_Update);
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM2, ENABLE);
}

void TIM2_IRQHandler(void)
{
	uint8_t i;
	if(TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
		g_tick++;

		{
			static const int8_t tbl[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
			uint8_t idr = (uint8_t)GPIOB->IDR;
			uint8_t cur = (uint8_t)(((idr>>6)&1)<<1) | (uint8_t)((idr>>5)&1);
			uint8_t idx = (enc_last<<2) | cur;
			g_enc_pos += tbl[idx];
			enc_last = cur;
		}

		for(i=0;i<4;i++)
		{
			uint8_t raw;
			switch(i)
			{
				case 0: raw = K1?0:1; break;
				case 1: raw = K2?0:1; break;
				case 2: raw = K3?0:1; break;
				default: raw = EN_SW?0:1; break;
			}

			if(raw != kb[i])
			{
				kc[i]++;
				if(kc[i] >= 3)
				{
					kc[i] = 0;
					kb[i] = raw;
					if(raw)
					{
						kpress[i] = g_tick;
						kheld[i] = 1;
						klong[i] = 0;
					}
					else
					{
						if(kheld[i] && !klong[i])
						{
							ev_short(i);
						}
						kheld[i] = 0;
					}
				}
			}
			else
			{
				kc[i] = 0;
			}

			if(kheld[i] && !klong[i] && (g_tick-kpress[i]) >= long_t[i])
			{
				ev_long(i);
				klong[i] = 1;
			}
		}
	}
}
