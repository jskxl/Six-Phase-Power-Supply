#include "eeprom.h"
#include "delay.h"

#define SCL_H()  GPIO_SetBits(GPIOB, GPIO_Pin_12)
#define SCL_L()  GPIO_ResetBits(GPIOB, GPIO_Pin_12)
#define SDA_H()  GPIO_SetBits(GPIOB, GPIO_Pin_13)
#define SDA_L()  GPIO_ResetBits(GPIOB, GPIO_Pin_13)
#define SDA_READ()  ((GPIOB->IDR & GPIO_Pin_13) ? 1 : 0)

static void i2c_delay(void)
{
	delay_us(10);
}

static void sda_out(void)
{
	GPIO_InitTypeDef g;
	g.GPIO_Pin = GPIO_Pin_13;
	g.GPIO_Mode = GPIO_Mode_Out_PP;
	g.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &g);
}

static void sda_in(void)
{
	GPIO_InitTypeDef g;
	g.GPIO_Pin = GPIO_Pin_13;
	g.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOB, &g);
}

void EEPROM_Init(void)
{
	GPIO_InitTypeDef g;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	g.GPIO_Pin  = GPIO_Pin_12;
	g.GPIO_Mode = GPIO_Mode_Out_PP;
	g.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &g);
	g.GPIO_Pin  = GPIO_Pin_13;
	g.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &g);
	SCL_H();
	SDA_H();
}

static void i2c_start(void)
{
	sda_out();
	SDA_H(); SCL_H(); i2c_delay();
	SDA_L(); i2c_delay();
	SCL_L(); i2c_delay();
}

static void i2c_stop(void)
{
	sda_out();
	SDA_L(); SCL_H(); i2c_delay();
	SDA_H(); i2c_delay();
}

static u8 i2c_write_byte(u8 dat)
{
	u8 i, ack;
	sda_out();
	for(i=0;i<8;i++)
	{
		if(dat & 0x80) SDA_H(); else SDA_L();
		dat <<= 1;
		i2c_delay();
		SCL_H(); i2c_delay();
		SCL_L(); i2c_delay();
	}
	sda_in();
	i2c_delay();
	SCL_H(); i2c_delay();
	ack = SDA_READ() ? 0 : 1;
	SCL_L(); i2c_delay();
	return ack;
}

static u8 i2c_read_byte(u8 ack)
{
	u8 i, dat = 0;
	sda_in();
	for(i=0;i<8;i++)
	{
		i2c_delay();
		SCL_H(); i2c_delay();
		dat = (u8)((dat<<1) | SDA_READ());
		SCL_L(); i2c_delay();
	}
	sda_out();
	if(ack) SDA_L(); else SDA_H();
	i2c_delay();
	SCL_H(); i2c_delay();
	SCL_L(); i2c_delay();
	SDA_H();
	return dat;
}

static u8 ee_wait_write(void)
{
	u8 i;
	for(i=0;i<30;i++)
	{
		IWDG->KR = 0xAAAA;
		delay_ms(1);
		i2c_start();
		if(i2c_write_byte(EEPROM_DEV_ADDR)) { i2c_stop(); return 1; }
		i2c_stop();
	}
	return 0;
}

u8 EEPROM_Write(u16 addr, u8 *buf, u8 len)
{
	u8 retry;
	u8 j;
	u8 chunk;
	u16 pos;
	u8 ok;
	for(retry=0;retry<3;retry++)
	{
		pos=0;
		ok=1;
		while(pos<len && ok)
		{
			chunk=(u8)(8-((addr+pos)&0x07));
			if(chunk>(u8)(len-pos))chunk=(u8)(len-pos);
			i2c_start();
			if(!i2c_write_byte(EEPROM_DEV_ADDR))   { ok=0; break; }
			if(!i2c_write_byte((u8)((addr+pos)&0xFF))) { ok=0; break; }
			for(j=0;j<chunk;j++)
			{
				if(!i2c_write_byte(buf[pos+j]))    { ok=0; break; }
			}
			i2c_stop();
			if(!ee_wait_write()) { ok=0; break; }
			pos=(u16)(pos+chunk);
		}
		if(ok && pos==len) return 1;
	}
	return 0;
}

u8 EEPROM_Read(u16 addr, u8 *buf, u8 len)
{
	u8 i;
	i2c_start();
	if(!i2c_write_byte(EEPROM_DEV_ADDR))   { i2c_stop(); return 0; }
	if(!i2c_write_byte((u8)(addr & 0xFF))) { i2c_stop(); return 0; }
	i2c_start();
	if(!i2c_write_byte(EEPROM_DEV_ADDR|1))  { i2c_stop(); return 0; }
	for(i=0;i<len;i++) buf[i] = i2c_read_byte(i < (u8)(len-1));
	i2c_stop();
	return 1;
}
