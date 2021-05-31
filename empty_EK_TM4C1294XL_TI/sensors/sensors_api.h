/*
 * sensor_api.h
 *
 *  Created on: 30 May 2021
 *      Author: Annie
 */

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

#include "opt3001/opt3001.h"
#include "Board.h"

void swiLight();

void taskLight();

bool initLightTask();

// returns filtered light lux value
float getLight();

//bool initLightSensor();

void swiAcceleration();
void taskAcceleration();
float getAcceleration();
void setAccelThreshold(uint16_t thresh);  // Use m/s^2
void accelEStopIntterupt();
bool initAcceleration(uint16_t thresholdAccel);
bool initSensors(uint16_t thresholdAccel);

#endif /* SENSORS_SENSORS_API_H_ */
