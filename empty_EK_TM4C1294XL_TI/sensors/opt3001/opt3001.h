/*
 * opt3001.h
 *
 *  Created on: 30 May 2021
 *      Author: Annie
 */

#ifndef SENSORS_OPT3001_OPT3001_H_
#define SENSORS_OPT3001_OPT3001_H_

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <xdc/runtime/System.h>
#include <ti/drivers/I2C.h>

// enables or disables OPT3001
void OPT3001Enable(I2C_Handle i2c, bool enable);

// tests opt3001 connection
bool OPT3001Test(I2C_Handle i2c);

// reads from OPT3001 light register
bool OPT3001Read(I2C_Handle i2c, uint16_t *rawData);

// converts raw light data into lux value
void OPT3001Convert(uint16_t rawData, float *convertedLux);

// sends i2c data to slave address and specified registers
bool OPT3001WriteI2C(I2C_Handle i2c, uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *Data);

// reads i2c data from slave address and specified registers
bool OPT3001ReadI2C(I2C_Handle i2c, uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *Data);

#endif /* SENSORS_OPT3001_OPT3001_H_ */
