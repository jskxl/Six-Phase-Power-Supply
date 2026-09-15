#include "lcd.h"
#include "font_ui.h"
#include "delay.h"

u16 BACK_COLOR, POINT_COLOR;

void LCD_Writ_Bus(u8 dat)
{
	while((SPI1->SR & SPI_SR_TXE) == 0);
	SPI1->DR = dat;
	while((SPI1->SR & SPI_SR_BSY) != 0);
}

void LCD_WR_DATA8(u8 da)
{
	LCD_DC_Set();
	LCD_Writ_Bus(da);
}

void LCD_WR_DATA(u16 da)
{
	LCD_DC_Set();
	LCD_Writ_Bus((u8)(da>>8));
	LCD_Writ_Bus((u8)da);
}

void LCD_WR_REG(u8 da)
{
	LCD_DC_Clr();
	LCD_Writ_Bus(da);
}

void Address_set(u16 x1,u16 y1,u16 x2,u16 y2)
{
	LCD_WR_REG(0x2a);
	LCD_WR_DATA8((u8)(x1>>8));
	LCD_WR_DATA8((u8)x1);
	LCD_WR_DATA8((u8)(x2>>8));
	LCD_WR_DATA8((u8)x2);

	LCD_WR_REG(0x2b);
	LCD_WR_DATA8((u8)(y1>>8));
	LCD_WR_DATA8((u8)y1);
	LCD_WR_DATA8((u8)(y2>>8));
	LCD_WR_DATA8((u8)y2);

	LCD_WR_REG(0x2C);
}

void Lcd_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	SPI_InitTypeDef SPI_InitStructure;

						RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB|RCC_APB2Periph_SPI1, ENABLE);

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_5|GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_Pin_4);

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0|GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_0|GPIO_Pin_1);

	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	SPI_Init(SPI1, &SPI_InitStructure);
	SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);
	SPI_Cmd(SPI1, ENABLE);

	LCD_RST_Clr();
	delay_ms(20);
	LCD_RST_Set();
	delay_ms(20);
	LCD_BLK_Set();

	LCD_WR_REG(0x36);
	LCD_WR_DATA8(0x60);

	LCD_WR_REG(0x3A);
	LCD_WR_DATA8(0x05);

	LCD_WR_REG(0xB2);
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x33);

	LCD_WR_REG(0xB7);
	LCD_WR_DATA8(0x35);

	LCD_WR_REG(0xBB);
	LCD_WR_DATA8(0x19);

	LCD_WR_REG(0xC0);
	LCD_WR_DATA8(0x2C);

	LCD_WR_REG(0xC2);
	LCD_WR_DATA8(0x01);

	LCD_WR_REG(0xC3);
	LCD_WR_DATA8(0x12);

	LCD_WR_REG(0xC4);
	LCD_WR_DATA8(0x20);

	LCD_WR_REG(0xC6);
	LCD_WR_DATA8(0x0F);

	LCD_WR_REG(0xD0);
	LCD_WR_DATA8(0xA4);
	LCD_WR_DATA8(0xA1);

	LCD_WR_REG(0xE0);
	LCD_WR_DATA8(0xD0);
	LCD_WR_DATA8(0x04);
	LCD_WR_DATA8(0x0D);
	LCD_WR_DATA8(0x11);
	LCD_WR_DATA8(0x13);
	LCD_WR_DATA8(0x2B);
	LCD_WR_DATA8(0x3F);
	LCD_WR_DATA8(0x54);
	LCD_WR_DATA8(0x4C);
	LCD_WR_DATA8(0x18);
	LCD_WR_DATA8(0x0D);
	LCD_WR_DATA8(0x0B);
	LCD_WR_DATA8(0x1F);
	LCD_WR_DATA8(0x23);

	LCD_WR_REG(0xE1);
	LCD_WR_DATA8(0xD0);
	LCD_WR_DATA8(0x04);
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x11);
	LCD_WR_DATA8(0x13);
	LCD_WR_DATA8(0x2C);
	LCD_WR_DATA8(0x3F);
	LCD_WR_DATA8(0x44);
	LCD_WR_DATA8(0x51);
	LCD_WR_DATA8(0x2F);
	LCD_WR_DATA8(0x1F);
	LCD_WR_DATA8(0x1F);
	LCD_WR_DATA8(0x20);
	LCD_WR_DATA8(0x23);

	LCD_WR_REG(0x21);
	LCD_WR_REG(0x11);
	delay_ms(120);   /* 退出睡眠后等待内部稳定再开显示 */
	LCD_WR_REG(0x29);
}

void LCD_Clear(u16 Color)
{
	u16 i,j;
	Address_set(0,0,LCD_W-1,LCD_H-1);
	for(i=0;i<LCD_W;i++)
	{
		for(j=0;j<LCD_H;j++)
		{
			LCD_WR_DATA(Color);
		}
	}
}

void LCD_DrawPoint(u16 x,u16 y)
{
	Address_set(x,y,x,y);
	LCD_WR_DATA(POINT_COLOR);
}

void LCD_Fill(u16 xsta,u16 ysta,u16 xend,u16 yend,u16 color)
{
	u16 i,j;
	Address_set(xsta,ysta,xend,yend);
	for(i=ysta;i<=yend;i++)
	{
		for(j=xsta;j<=xend;j++) LCD_WR_DATA(color);
	}
}

void LCD_DrawLine(u16 x1,u16 y1,u16 x2,u16 y2)
{
	u16 t;
	int xerr=0,yerr=0,delta_x,delta_y,distance;
	int incx,incy,uRow,uCol;

	delta_x=x2-x1;
	delta_y=y2-y1;
	uRow=x1;
	uCol=y1;
	if(delta_x>0)incx=1;
	else if(delta_x==0)incx=0;
	else {incx=-1;delta_x=-delta_x;}
	if(delta_y>0)incy=1;
	else if(delta_y==0)incy=0;
	else{incy=-1;delta_y=-delta_y;}
	if(delta_x>delta_y)distance=delta_x;
	else distance=delta_y;
	for(t=0;t<=distance+1;t++)
	{
		LCD_DrawPoint(uRow,uCol);
		xerr+=delta_x;
		yerr+=delta_y;
		if(xerr>distance){xerr-=distance;uRow+=incx;}
		if(yerr>distance){yerr-=distance;uCol+=incy;}
	}
}

void LCD_DrawRectangle(u16 x1,u16 y1,u16 x2,u16 y2)
{
	LCD_DrawLine(x1,y1,x2,y1);
	LCD_DrawLine(x1,y1,x1,y2);
	LCD_DrawLine(x1,y2,x2,y2);
	LCD_DrawLine(x2,y1,x2,y2);
}

void Draw_Circle(u16 x0,u16 y0,u8 r)
{
	int a,b,di;
	a=0;b=r;
	di=3-(r<<1);
	while(a<=b)
	{
		LCD_DrawPoint(x0-b,y0-a);
		LCD_DrawPoint(x0+b,y0-a);
		LCD_DrawPoint(x0-a,y0+b);
		LCD_DrawPoint(x0-b,y0-a);
		LCD_DrawPoint(x0-a,y0-b);
		LCD_DrawPoint(x0+b,y0+a);
		LCD_DrawPoint(x0+a,y0-b);
		LCD_DrawPoint(x0+a,y0+b);
		LCD_DrawPoint(x0-b,y0+a);
		a++;
		if(di<0)di+=4*a+6;
		else
		{
			di+=10+4*(a-b);
			b--;
		}
		LCD_DrawPoint(x0+a,y0+b);
	}
}

/* 8x16 ASCII, mode: 0=带背景 1=不带背景 */
void LCD_ShowChar(u16 x,u16 y,u8 num,u8 mode)
{
	u8 temp,pos,t;
	u16 x0=x;
	u16 colortemp=POINT_COLOR;
	if(x>LCD_W-16||y>LCD_H-16)return;
	num=num-' ';
	Address_set(x,y,x+8-1,y+16-1);
	if(!mode)
	{
		for(pos=0;pos<16;pos++)
		{
			temp=asc2_1608[(u16)num*16+pos];
			for(t=0;t<8;t++)
			{
				if(temp&0x01)POINT_COLOR=colortemp;
				else POINT_COLOR=BACK_COLOR;
				LCD_WR_DATA(POINT_COLOR);
				temp>>=1;
				x++;
			}
			x=x0;
			y++;
		}
	}
	else
	{
		for(pos=0;pos<16;pos++)
		{
			temp=asc2_1608[(u16)num*16+pos];
			for(t=0;t<8;t++)
			{
				if(temp&0x01)LCD_DrawPoint(x+t,y+pos);
				temp>>=1;
			}
		}
	}
	POINT_COLOR=colortemp;
}

void LCD_ShowString(u16 x,u16 y,const u8 *p)
{
	while(*p!='\0')
	{
		if(x>LCD_W-16){x=0;y+=16;}
		if(y>LCD_H-16){y=x=0;LCD_Clear(RED);}
		LCD_ShowChar(x,y,*p,0);
		x+=8;
		p++;
	}
}


/* m^n */
u32 mypow(u8 m,u8 n)
{
	u32 result=1;
	while(n--)result*=m;
	return result;
}

/* 显示数字: len位, 高位为0显示空格 */
void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len)
{
	u8 t,temp;
	u8 enshow=0;
	for(t=0;t<len;t++)
	{
		temp=(u8)((num/mypow(10,len-t-1))%10);
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				LCD_ShowChar(x+8*t,y,' ',0);
				continue;
			}
			else enshow=1;
		}
		LCD_ShowChar(x+8*t,y,(u8)(temp+48),0);
	}
}
/* 固定两位小数数值显示: centi=实际值*100, 整数部分按实际值 */
void LCD_ShowNumCenti(u16 x,u16 y,u16 centi,u8 mode)
{
	u8 buf[8];
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
	for(i=0;i<n;i++)
	{
		LCD_ShowChar(x,y,buf[i],mode);
		x+=8;
	}
}

/* 16x16 汉字 */
void LCD_ShowHanzi16(u16 x,u16 y,u8 idx)
{
	u8 r,b,t;
	if(idx>46)return;
	if(x>LCD_W-16||y>LCD_H-16)return;
	Address_set(x,y,x+15,y+15);
	for(r=0;r<16;r++)
	{
		for(b=0;b<2;b++)
		{
			u8 d=font16[idx*32+r*2+b];
			for(t=0;t<8;t++)
			{
				if(d&(0x80>>t))LCD_WR_DATA(POINT_COLOR);
				else LCD_WR_DATA(BACK_COLOR);
			}
		}
	}
}

/* 12x12 汉字 */
void LCD_ShowHanzi12(u16 x,u16 y,u8 idx)
{
	u8 r,t;
	if(idx>46)return;
	if(x>LCD_W-12||y>LCD_H-12)return;
	Address_set(x,y,x+11,y+11);
	for(r=0;r<12;r++)
	{
		u8 d0=font12[idx*24+r*2+0];
		u8 d1=font12[idx*24+r*2+1];
		for(t=0;t<8;t++)
		{
			if(d0&(0x80>>t))LCD_WR_DATA(POINT_COLOR);
			else LCD_WR_DATA(BACK_COLOR);
		}
		for(t=0;t<4;t++)
		{
			if(d1&(0x80>>t))LCD_WR_DATA(POINT_COLOR);
			else LCD_WR_DATA(BACK_COLOR);
		}
	}
}

/* 6x8 ASCII */
void LCD_ShowAscii68(u16 x,u16 y,const char *s)
{
	u8 r,t,n;
	while(*s)
	{
		n=(u8)(*s)-32;
		if(n>94)n=0;
		if(x<=LCD_W-6)
		{
			Address_set(x,y,x+5,y+7);
			for(r=0;r<8;r++)
			{
				u8 d=font68[n*8+r];
				for(t=0;t<6;t++)
				{
					if(d&(0x80>>t))LCD_WR_DATA(POINT_COLOR);
					else LCD_WR_DATA(BACK_COLOR);
				}
			}
		}
		x+=6;
		s++;
	}
}

/* 7段大数字 14x28 */
void LCD_ShowBig(u16 x,u16 y,u8 idx)
{
	u8 r,t;
	if(idx>12)return;
	if(x>LCD_W-14||y>LCD_H-28)return;
	Address_set(x,y,x+13,y+27);
	for(r=0;r<28;r++)
	{
		u8 d0=font_big[idx*56+r*2+0];
		u8 d1=font_big[idx*56+r*2+1];
		for(t=0;t<8;t++)
		{
			if(d0&(0x80>>t))LCD_WR_DATA(POINT_COLOR);
			else LCD_WR_DATA(BACK_COLOR);
		}
		for(t=0;t<6;t++)
		{
			if(d1&(0x80>>t))LCD_WR_DATA(POINT_COLOR);
			else LCD_WR_DATA(BACK_COLOR);
		}
	}
}
