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
//uint8_t setBand(uint8_t addr, uint8_t bandindex);

//band switch LUT
uint8_t bandmodes[] = {0, 0b0001, 0b0010, 0b0011, 0b0100, 0b0111, 0b00010000, 0b10000000, 0b10001000, 0b10000001, 0b10000010, 0b10000100, 0xff};
#define BANDS (sizeof(bandmodes))

//div. stuff
#define MENUSIZE 4
#define NUMPLLS 2
char * menutext[] = {"band", "pump", "highcur", "test0"};
pll_settings pll[NUMPLLS];

/*uint8_t setBand(uint8_t addr, uint8_t bandindex)
{
	return pll_update_ctrl(0xC2, PLL_C1_DEFAULT, bandmodes[bandindex]);
}*/


void menu_command(char *output, uint8_t cmd, uint8_t num_pll)
{
	uint8_t value;
	if(cmd > MENUSIZE || num_pll > NUMPLLS-1) return;
	pll_settings *p = &pll[num_pll];

	switch(cmd)
	{
		case 0:
			p->c2 = inc_band_mask(p->c2);
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

		default:
			return;
	}

	sprintf(output, "0x%X %s: %i, a: %s\r\n", p->addr, menutext[cmd], value, pll_update_ctrl(p)?"ok":"FAIL");
}


void menu_loop()
{
	uint8_t menu = 0;
	uint16_t div = 10082, lastdiv = div;
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
				menu_command(buf, menu>>1, menu % NUMPLLS);
				uart_putstr(buf);
				_delay_ms(100);
				break;

			case _BV(PB2):
				div -= 2;
				_delay_ms(50);
				break;

			case _BV(PB3):
				div += 2;
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
			sprintf(buf, "div: %i, %s\r\n", div, (pll_update_all(&pll[0]) & pll_update_all(&pll[1]))?"ok":"FAIL");
			uart_putstr(buf);
			_delay_us(500);
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
	pll[0].div = 10082;
	pll_update_all(&pll[0]);

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

	//init serial port to 19200bps
	uart_init();

	//setup PLLs to normal operation
	pll_init(&pll[0], 0xC2, 256);
	pll_init(&pll[1], 0xC6, 256);

	//enable keypad pullups
	PORTB |=  0x0F;

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
