/*
 * empty.h
 *
 *  Created on: 28 May 2021
 *      Author: rajsi
 */

#include <xdc/std.h>
#include "stdbool.h"

#ifndef MOTORCODE_H_
#define MOTORCODE_H_


/*!
 *  @brief  ISR to read the Hall Effect lines, calculate RPM and update the motor
 */
void HallFxn(void);

/*!
 *  @brief  Setup all HWIs for each GPIO line, for Hall A/B/C, and enable interrupts
 */
void initHallABC(void);

/*!
 *  @brief  Read the current state of the hall effect sensors
 */
void readABC(void);

/*!
 *  @brief  initialise the UART. Only used during development, not required
 *  during production.
 */
void initUART(void);

/*!
 *  @brief  Task thread for the motor. Responsible for calling the HWI setup and driving the
 *          initial updateMotor. A while loop is used to control the states of the system
 *          (speed up, speed down, e-stop). UART was used during testing and is not required
 *          for production.
 *
 *  @params arg0, arg1
 */
void motorFxn(UArg arg0, UArg arg1);


/*!
 *  @brief  Filter - Calculates the average RPM from the actual RPM calculated
 *          by the HWI. Uses a sample of SAMPLE_SIZE at a frequency
 *          of 100Hz (clock period 10ms).
 *
 *  @params arg0
 */
void filterFxn(UArg arg0);

/*!
 *  @brief  Proportional Integral Control for the speed, with a frequency of 100Hz.
 *
 *  @params arg0
 */
void PIControlFxn(UArg arg0);

/*!
 *  @brief Simulates UI speed increase. Used for testing.
 *
 *  @params arg0
 */
void speedFxn(UArg arg0);

/*!
 *  @brief Limits the acceleration of the motor to 500RPM/s. This Clock runs has a
 *         frequency of 100Hz, and uses an intermediate RPM that increments at
 *         5RPM every 0.01s - results in an acceleration of 500RPM/s.
 *
 *  @params arg0
 */
void accelLimitFxn(UArg arg0);
void waitFxn(UArg arg0);
//void ADC0_Init();

void userStart(void);
bool getState(void);

void set_eStop(void);

/*!
 *  @brief Gets the current speed of the motor
 */
int getSpeed(void);

/*!
 *  @brief Sets the desired RPM from the UI.
 *
 *  @params rpm_ui  the UI selected RPM
 */
void setSpeed(uint32_t rpm_UI);


/*!
 *  @brief sets the motor start condition
 *
 *  @params UI      the boolean condition to start the motor
 */
void startMotor(bool motorRun);

/*!
 *  @brief  Initialise the motorCode file.
 *
 *  @params arg0
 */
void initMotor(void);

#endif /* MOTORCODE_H_ */
