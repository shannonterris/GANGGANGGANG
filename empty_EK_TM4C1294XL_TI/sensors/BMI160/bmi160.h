/*
 * bmi160.h
 *
 *  Created on: 31 May 2021
 *      Author: Annie
 */

#ifndef SENSORS_BMI160_BMI160_H_
#define SENSORS_BMI160_BMI160_H_

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

#include <inc/hw_ints.h>
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_gpio.h>

#include <driverlib/flash.h>
#include <driverlib/gpio.h>
#include <driverlib/i2c.h>
#include <driverlib/pin_map.h>
#include <driverlib/pwm.h>
#include <driverlib/ssi.h>
#include <driverlib/sysctl.h>
#include <driverlib/uart.h>
#include <driverlib/udma.h>

#include "EK_TM4C1294XL.h"

#include <stdbool.h>
#include <stdint.h>
#include <ti/drivers/I2C.h>
#include <inc/hw_memmap.h>
#include <ti/sysbios/knl/Task.h>

typedef struct {
    float x, y, z;
} accel_vector;

bool BMI160Init(I2C_Handle i2c);
bool accelThreshold(I2C_Handle i2c, uint8_t threshold);
bool getAccelVector(I2C_Handle i2c, accel_vector *av);
bool writeI2C(I2C_Handle i2c, uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *data);
bool readI2C(I2C_Handle i2c, uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *data);



#endif /* SENSORS_BMI160_BMI160_H_ */
