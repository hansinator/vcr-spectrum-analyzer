#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <util/delay.h>     /* for _delay_ms() */
#include <stdio.h>
#include <string.h>
#include "i2csw.h"
#include "uart.h"
#include "pll.h"

void init();
uint8_t inc_band_mask(uint8_t mask);
uint8_t set_scart_mux(uint8_t sw, uint8_t pos);

//band switch LUT
uint8_t bandmodes[] = {0, 0b11, 0b111};
#define BANDS (sizeof(bandmodes))

//div. stuff
#define MENUSIZE 5
#define NUMPLLS 2
char * menutext[] = {"band", "pump", "highcur", "test0", "tunespeed"};
pll_settings pll[NUMPLLS];
uint8_t tunespeed;
uint8_t band = 0;

#define DEFAULT_DIV 8670

void menu_pll(char *output, uint8_t cmd, uint8_t num_pll)
{
	uint8_t value;
	if(cmd > MENUSIZE || num_pll > NUMPLLS-1) return;
	pll_settings *p = &pll[num_pll];

	switch(cmd)
	{
		case 0:
			//p->c2 = inc_band_mask(p->c2);
			p->c2 = bandmodes[band++];
			if(band >= BANDS) band = 0;
			value = p->c2 & 0b10010111;
			break;

		case 1:
			p->c1 ^= _BV(PLL_DISABLE_PUMP);
			value = ~p->c1 & _BV(PLL_DISABLE_PUMP);
			break;

		case 2:
			p->c1 ^= _BV(PLL_PUMPCURRENT);
			value = p->c1 & _BV(PLL_PUMPCURRENT);
			break;

		case 3:
			p->c1 ^= _BV(PLL_TEST0);
			value = p->c1 & _BV(PLL_TEST0);
			break;

		case 4:
			tunespeed <<= 1;
			if(tunespeed == 0) tunespeed = 1;
			value = tunespeed;
			break;

		default:
			return;
	}

	sprintf(output, "0x%X %s: %i, a: %s\r\n", p->addr, menutext[cmd], value, pll_update_ctrl(p)?"ok":"FAIL");
}

uint8_t swpos = 0;
void menu_loop()
{
	uint8_t menu = 0;
	uint16_t div = DEFAULT_DIV, lastdiv = div;
	char buf[50];

	while(23 != 42)
	{
		//poll keypad
		switch(~PINB & 0x0F)
		{
			case _BV(PB0):
				menu++;
				menu %= MENUSIZE * NUMPLLS;
				sprintf(buf, "0x%X %s menu\r\n", pll[menu%2].addr, menutext[menu>>1]);
				uart_putstr(buf);
				_delay_ms(100);
				break;

			case _BV(PB1):
				menu_pll(buf, menu>>1, menu % NUMPLLS);
				uart_putstr(buf);
				_delay_ms(100);
				break;

			case _BV(PB2):
				div -= tunespeed;
				_delay_ms(50);
				break;

			case _BV(PB3):
				div += tunespeed;
				_delay_ms(50);
				break;
		}

		//wrap PLL divider
		if(div < PLL_DIV_MIN)
			div = PLL_DIV_MAX;
		else if(div > PLL_DIV_MAX)
			div = PLL_DIV_MIN;

		//update pll frequencies
		if(div != lastdiv)
		{
			lastdiv = div;
			pll[0].div = div;
			pll[1].div = div;
			sprintf(buf, "div: %i, %s\r\n", div, (pll_update_all(&pll[0]) & pll_update_all(&pll[1]))?"ok":"FAIL");
			uart_putstr(buf);
			_delay_us(500);
		}

		if(uart_getc_nb(buf))
		{
			switch(*buf)
			{
				case 'x':
					swpos++;
					break;

				case 'y':
					swpos--;
					break;

				default:
					break;
			}
			swpos &= 7;
			sprintf(buf, "mux: %i %i %i\r\n", (swpos & 4)?1:0, (swpos & 2)?1:0, (swpos & 1)?1:0);
			uart_putstr(buf);
			set_scart_mux(0, swpos);
			set_scart_mux(1, swpos);
			set_scart_mux(2, swpos);
			set_scart_mux(3, swpos);
		}
	}
}


uint8_t inc_band_mask(uint8_t mask)
{
	if(mask == 0b111)
		return 0b10000;
	else if(mask == 0b10111)
		return 0b10000000;
	else if(mask == 0b10010111)
		return 0xFF;
	else
		return mask+1;
}


void probe_addresses()
{
	uint8_t i;
	char buf[50];

	uart_putstr("Probing addresses..\r\n");
	for(i=0; i<255; i++)
	{
		if(i2cProbe(i))
		{
			sprintf(buf, "found 0x%X\r\n", i);
			uart_putstr(buf);
		}
		_delay_ms(1);
	}
	uart_putstr("Done!\r\n\r\n");
}


void sniffer_loop()
{
	uint8_t count, buf[32];

	while(!0)
	{
		count = ic2sniff(buf, sizeof(buf)-1);
		uart_putstr("packet\r\n");
		uart_hexdump(buf, count);
		uart_putstr("\r\n");
	}
}

uint8_t set_scart_mux(uint8_t sw, uint8_t pos)
{
	uint8_t ack = 1;
	i2cStart();
	ack &= i2cPutbyte(0x92);
	ack &= i2cPutbyte(((sw & 3) << 4) | (pos & 7));
	i2cStop();

	return ack;
}

#define FP_ADDR 0x10
#define DSP_ADDR 0x12

uint8_t write_dsp(uint16_t addr, uint16_t value)
{
	uint8_t ack = 1;
	i2cStart();
	ack &= i2cPutbyte(0x80);
	ack &= i2cPutbyte(DSP_ADDR);
	ack &= i2cPutbyte(addr >> 8);
	ack &= i2cPutbyte(addr & 0xFF);
	ack &= i2cPutbyte(value >> 8);
	ack &= i2cPutbyte(value & 0xFF);
	i2cStop();

	return ack;
}
uint8_t write_fp(uint16_t addr, uint16_t value)
{
	uint8_t ack = 1;
	i2cStart();
	ack &= i2cPutbyte(0x80);
	ack &= i2cPutbyte(FP_ADDR);
	ack &= i2cPutbyte(addr >> 8);
	ack &= i2cPutbyte(addr & 0xFF);
	ack &= i2cPutbyte(value >> 8);
	ack &= i2cPutbyte(value & 0xFF);
	i2cStop();

	return ack;
}

#define FP_AD_CV 0x00BB
#define FP_MODE_REG 0x0083
#define FP_FIR_REG_1 0x001
#define FP_FIR_REG_2 0x005
#define FP_DCO1_LO 0x0093
#define FP_DCO1_HI 0x009B
#define FP_DCO2_LO 0x00A3
#define FP_DCO2_HI 0x00AB
#define FP_FAWCT_SOLL 0x0107
#define FP_FAW_ER_TOL  0x010F
#define FP_AUDIO_PLL 0x02D7
#define FP_CMD_LOAD_REG_1_2 0x0056
#define FP_CMD_LOAD_REG_1 0x0060
#define FP_CMD_SEARCH_NICAM 0x0078
#define FP_CMD_SELF_TEXT 0x0792

uint8_t fir_german_dual_fm[] = {3, 18, 27, 48, 66, 72};

//general initialization of demodulator
uint8_t init_fp()
{
	uint8_t i, ack = 1;

	//set stuff
	ack &= write_fp(FP_AD_CV, 32 | 1 << 8); //16.6dB constant gain, analog in 2
	ack &= write_fp(FP_AUDIO_PLL, 1); //close pll

	//setup faw with suggested values (not using nicam anyway)
	ack &= write_fp(FP_FAWCT_SOLL, 12);
	ack &= write_fp(FP_FAW_ER_TOL, 2);

	//fir
	for(i = sizeof(fir_german_dual_fm)-1; i >= 0; i--)
		ack &= write_fp(FP_FIR_REG_1, fir_german_dual_fm[i]);
	for(i = sizeof(fir_german_dual_fm)-1; i >= 0; i--)
		ack &= write_fp(FP_FIR_REG_2, fir_german_dual_fm[i]);

	//set all mode flags to fm processing
	ack &= write_fp(FP_MODE_REG, 1<<7);

	//write dco values
	ack &= write_fp(FP_DCO1_HI, 0x04C6);
	ack &= write_fp(FP_DCO1_LO, 0x038E);
	ack &= write_fp(FP_DCO2_HI, 0x04C6);
	ack &= write_fp(FP_DCO2_LO, 0x038E);

	//load registers
	ack &= write_fp(FP_CMD_LOAD_REG_1_2, 0);

	return ack;
}

#define DSP_VOL_LOUD 0x0000
#define DSP_VOL_HEAD 0x0009
#define DSP_VOL_SCART 0x0007

void init_dsp()
{
	write_dsp(DSP_VOL_LOUD, 0x73<<8);
	write_dsp(DSP_VOL_SCART, 0x40<<8);
	write_dsp(DSP_VOL_HEAD, 0x73<<8);
	write_dsp(0x000E, 0x30<<8);
	write_dsp(0x000D, 0x19<<8);
}

int main(void)
{
	//system initialization
	init();
	uart_putstr("Hi there!\r\n\r\n");

	//probe addresses
	probe_addresses();

	//sniff bus traffic
	//sniffer_loop();

	//pretune channel 36 (591.2MHz)
	pll[0].c1 &= ~_BV(PLL_DISABLE_PUMP);
	pll[0].div = DEFAULT_DIV;
	pll[1].c1 = 0x9f;
	pll[1].c2 = 0x0;
	pll[1].div = 0x12f4;
	pll[1].c1 &= ~_BV(PLL_DISABLE_PUMP);
	pll[1].div = DEFAULT_DIV;
	pll_update_all(&pll[0]);

	//init sound processor
	init_fp();
	init_dsp();

	//test bands
	//try_all_bands(10);

	/*//test bands
	try_bands(0xC2, 100, 10082);
	try_bands(0xC6, 100, 10082);
	uart_putstr("Done!\r\n\r\n"); */

	menu_loop();
    return 0;
}


void init()
{
	//disable analog comparator (to save power)
	//ACSR |= _BV(ACD);

	tunespeed = 1;

	//init serial port to 19200bps
	uart_init();

	//setup PLLs to normal operation
	pll_init(&pll[0], 0xC2, 256);
	pll_init(&pll[1], 0xC6, 256);

	//enable keypad pullups
	PORTB |=  0x0F;

	//init scart mux
	set_scart_mux(0, 0);
	set_scart_mux(1, 0);
	set_scart_mux(2, 1); //tuner to lower scart
	set_scart_mux(3, 0);

	//enable interrupts
	sei();
}


/*void try_all_bands(uint8_t delay)
{
	uint8_t i, x;
	char buf[50];

	uart_putstr("Trying all bandswitches.. \r\n");

	i = 0;
	while(i < 255)
	{
		pll_update_ctrl(0xC2, PLL_C1_DEFAULT, i);
		sprintf(buf, "%i\r\n", i);
		uart_putstr(buf);
		x = 0;
		while(x < 255)
		{
			pll_update_ctrl(0xC6, PLL_C1_DEFAULT, x);
			_delay_ms(delay);
			x = inc_band_mask(x);
		}
		pll_update_ctrl(0xC6, PLL_C1_DEFAULT, x);
		_delay_ms(delay);
		i = inc_band_mask(i);
	}
	pll_update_ctrl(0xC2, PLL_C1_DEFAULT, i);
	while(x < 255)
	{
		pll_update_ctrl(0xC6, PLL_C1_DEFAULT, x);
		_delay_ms(delay);
		x = inc_band_mask(x);
	}
	pll_update_ctrl(0xC6, PLL_C1_DEFAULT, x);
	_delay_ms(delay);
	pll_update_ctrl(0xC2, PLL_C1_DEFAULT, 0);
	pll_update_ctrl(0xC6, PLL_C1_DEFAULT, 0);

	uart_putstr("Done!\r\n\r\n");
}


void try_bands(uint8_t addr, uint8_t delay, uint16_t div)
{
	uint8_t i = 0;
	char buf[50];

	sprintf(buf, "Trying bandswitches PLL 0x%X.. \r\n", addr);
	uart_putstr(buf);

	while(i < 255)
	{
		if(pll_update_all(addr, PLL_C1_DEFAULT, i, div) == 0)
			continue;
		_delay_ms(delay);
		i = inc_band_mask(i);
	}
	pll_update_ctrl(addr, PLL_C1_DEFAULT, i);
	_delay_ms(delay);
	pll_update_ctrl(addr, PLL_C1_DEFAULT, 0);
}*/
