/*
 * empty.h
 *
 *  Created on: 28 May 2021
 *      Author: rajsi
 */

#ifndef MOTORCODE_H_
#define MOTORCODE_H_

void HallFxn(void);
void initHallABC(void);
void readABC(void);
void motorFxn(UArg arg0, UArg arg1);
void clk0Fxn(UArg arg0);
void clk1Fxn(UArg arg0);
void speedFxn(UArg arg0);
void accelFxn(UArg arg0);
void ADC0_Init();
void initMotorCode(void);

#endif /* MOTORCODE_H_ */
