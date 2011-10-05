#ifndef _H_PLL
#define _H_PLL

#include <inttypes.h>

//control byte 1 flags
#define PLL_DISABLE_PUMP (0)
#define PLL_TEST0 (4)
#define PLL_TEST1 (5)
#define PLL_PUMPCURRENT (6)
//must be set in control byte 1 to indicate control register write
#define PLL_C11 (1)
#define PLL_C12 (7)

//min and max divider values (also useful for software band limiting)
#define PLL_DIV_MIN 256
#define PLL_DIV_MAX 16384

//control byte 1 default value
#define PLL_C1_DEFAULT (1<<PLL_DISABLE_PUMP)


typedef struct
{
	//address
	uint8_t addr;

	//frequency divider value
	uint16_t div;

	//control bytes 1 and 2
	uint8_t c1, c2;
} pll_settings;


uint8_t pll_update_divider(pll_settings *p);
uint8_t pll_update_ctrl(pll_settings *p);
uint8_t pll_update_all(pll_settings *p);
void pll_init(pll_settings *p, uint8_t addr, uint16_t div);

#endif
