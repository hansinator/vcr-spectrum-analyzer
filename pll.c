#include "pll.h"
#include "i2csw.h"

uint8_t pll_update_divider(pll_settings *p)
{
	uint8_t ack = 1;
	i2cStart();
	ack &= i2cPutbyte(p->addr);
	ack &= i2cPutbyte((p->div >> 8) & 0x7F);
	ack &= i2cPutbyte(p->div & 0xFF);
	i2cStop();

	return ack;
}


uint8_t pll_update_ctrl(pll_settings *p)
{
	uint8_t ack = 1;
	i2cStart();
	ack &= i2cPutbyte(p->addr);
	ack &= i2cPutbyte(p->c1 | (1<<PLL_C11) | (1<<PLL_C12));
	ack &= i2cPutbyte(p->c2);
	i2cStop();

	return ack;
}


uint8_t pll_update_all(pll_settings *p)
{
	uint8_t ack = 1;
	i2cStart();
	ack &= i2cPutbyte(p->addr);
	ack &= i2cPutbyte((p->div >> 8) & 0x7F); // must not be set
	ack &= i2cPutbyte(p->div & 0xFF);
	ack &= i2cPutbyte(p->c1 | (1<<PLL_C11) | (1<<PLL_C12));
	ack &= i2cPutbyte(p->c2);
	i2cStop();

	return ack;
}


void pll_init(pll_settings *p, uint8_t addr, uint16_t div)
{
	p->c1 = PLL_C1_DEFAULT;
	p->c2 = 0;
	p->div = div;
	p->addr = addr;
	pll_update_all(p);
}
