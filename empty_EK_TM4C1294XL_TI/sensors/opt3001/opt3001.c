/*
 * opt3001.c
 *
 *  Created on: 30 May 2021
 *      Author: Annie
 */

#include "opt3001.h"

#define OPT3001_I2C_ADDRESS             0x47

#define REG_CONFIGURATION               0x01
#define REG_RESULT                      0x00
#define REG_MANUFACTURER_ID             0x7E
#define REG_DEVICE_ID                   0x7F

#define MANUFACTURER_ID                 0x5449  // TI
#define DEVICE_ID                       0x3001  // Opt 3001

#define CONFIG_ENABLE                   0x10CE  // 0b0001000011001110   - 100 ms, continuous //0x10C4 // 0xC410   - 100 ms, continuous
#define CONFIG_DISABLE                  0x10C0  // 0xC010   - 100 ms, shutdown

#define DATA_RDY_BIT                    0x0080  // Data ready

void OPT3001Enable(I2C_Handle i2c, bool enable) {
    uint16_t val_config;

    // Starts as 0b11001 00 000010000
    // Enable by 0b11001110 00010000 MSB but swap order of bytes
    if (enable) {
        val_config = CONFIG_ENABLE;
    } else {
        val_config = CONFIG_DISABLE;
    }

    // Write to configuration register 01h
    OPT3001WriteI2C(i2c, OPT3001_I2C_ADDRESS, REG_CONFIGURATION, (uint8_t*)&val_config);
}

bool OPT3001Test(I2C_Handle i2c) {
    uint16_t val;

    // Check manufacturer ID
    OPT3001ReadI2C(i2c, OPT3001_I2C_ADDRESS, REG_MANUFACTURER_ID, (uint8_t *)&val);
    val = (val << 8) | (val>>8 &0xFF);

    if (val != MANUFACTURER_ID) {
        return false;
    }

    // Check device ID
    OPT3001ReadI2C(i2c, OPT3001_I2C_ADDRESS, REG_DEVICE_ID, (uint8_t *)&val);
    val = (val << 8) | (val>>8 &0xFF);

    if (val != DEVICE_ID) {
        return false;
    }
    return true;
}

bool OPT3001ReadLight(I2C_Handle i2c, uint16_t *rawData) {
    bool success;
    uint16_t val;

    success = OPT3001ReadI2C(i2c, OPT3001_I2C_ADDRESS, REG_CONFIGURATION, (uint8_t *)&val);

    if (success) {
        success = (val & DATA_RDY_BIT) == DATA_RDY_BIT;
    }
    if (success) {
        success = OPT3001ReadI2C(i2c, OPT3001_I2C_ADDRESS, REG_RESULT, (uint8_t *)&val);
    }
    if (success) {
        // Swap bytes
        *rawData = (val << 8) | (val>>8 &0xFF);
    }
    return success;
}

void OPT3001Convert(uint16_t rawData, float *convertedLux) {
    uint16_t e, m;

    m = rawData & 0x0FFF;
    e = (rawData & 0xF000) >> 12;
    *convertedLux = m * (0.01 * exp2(e));
}

bool OPT3001WriteI2C(I2C_Handle i2c, uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *data) {
    I2C_Transaction i2cTransaction;
    uint8_t txBuf[3];
    txBuf[0] = ui8Reg;
    txBuf[1] = data[0];
    txBuf[2] = data[1];

    i2cTransaction.slaveAddress = ui8Addr;
    i2cTransaction.writeBuf = txBuf;
    i2cTransaction.writeCount = 3;
    i2cTransaction.readBuf = NULL;
    i2cTransaction.readCount = 0;

    I2C_transfer(i2c, &i2cTransaction);
    return true;
}

bool OPT3001ReadI2C(I2C_Handle i2c, uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *data) {
    I2C_Transaction i2cTransaction;
    uint8_t txBuf[1];
    txBuf[0] = ui8Reg;

    i2cTransaction.slaveAddress = ui8Addr;
    i2cTransaction.writeBuf = txBuf;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = data;
    i2cTransaction.readCount = 2;

    I2C_transfer(i2c, &i2cTransaction);
    return true;
}


