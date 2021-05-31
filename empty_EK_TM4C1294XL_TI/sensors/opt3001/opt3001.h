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

// Enable or disable the OPT3001
void OPT3001Enable(I2C_Handle i2c, bool enable);

// Check if connected
bool OPT3001Test(I2C_Handle i2c);

// Read from OPT3001 light register
bool OPT3001ReadLight(I2C_Handle i2c, uint16_t *rawData);

// Convert data received from OPT3001 into lux
void OPT3001Convert(uint16_t rawData, float *convertedLux);

// Sends data along i2c to slave address and registers specified.
// Followed by two data bytes transferred over i2c.
bool OPT3001WriteI2C(I2C_Handle i2c, uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *Data);

// Sends data along i2c to slave address and registers specified.
// Followed by reading three data bytes transferred over i2c.
// Third read is redundant (used to flush i2c register).
bool OPT3001ReadI2C(I2C_Handle i2c, uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *Data);

#endif /* SENSORS_OPT3001_OPT3001_H_ */
