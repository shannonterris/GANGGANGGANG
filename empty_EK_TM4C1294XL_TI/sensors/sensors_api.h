#ifndef SENSORS_SENSORS_API_H_
#define SENSORS_SENSORS_API_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/UART.h>
#include <driverlib/adc.h>
#include <driverlib/sysctl.h>
#include <driverlib/gpio.h>
#include <inc/hw_memmap.h>
#include "BMI160/bmi160.h"
#include "board.h"

void swiAcceleration();
void taskAcceleration();
float getAcceleration();
void setAccelThreshold(uint16_t thresh);  // Use m/s^2
void accelEStopInterrupt();
bool initAcceleration(uint16_t thresholdAccel);
bool initSensors(uint16_t thresholdAccel);

#endif
