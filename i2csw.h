#ifndef I2CSW_H
#define I2CSW_H

#include <inttypes.h>
#define EE_DETECT 1
#define I2CSNIFF 1

#define SDA PA2
#define SDAPIN PINA
#define SDADDR DDRA
#define SCL PA1
#define SCLPIN PINA
#define SCLDDR DDRA

// functions

void i2cStart();

void i2cStop();


uint8_t i2cPutbyte(uint8_t b);

uint8_t i2cGetbyte(uint8_t last);


void i2cInit(void);

#ifdef I2CSNIFF
uint8_t ic2sniff(uint8_t *buf, uint8_t len);
#endif

//write to i2c eeprom in multibyte write mode
int8_t i2cEeWrite(uint16_t address, uint8_t *data, uint16_t len);

//read bytes from eeprom to buffer
void i2cEeRead(uint8_t *data, uint16_t address, uint16_t len);

//detect if there is a valid i2c eeprom card inserted
//returns card size in sectors of 256 bytes
//a return value of zero means no valid card
uint8_t i2cEeDetect();

#ifdef EE_DETECT
uint8_t i2cProbe(uint8_t addr);
#endif

#endif
