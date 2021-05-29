/*
 * grlib_demo.h
 *
 *  Created on: 28 May 2021
 *      Author: shann
 */

#ifndef GRLIB_DEMO_H_
#define GRLIB_DEMO_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/flash.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "driverlib/udma.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "grlib/grlib.h"
#include "grlib/widget.h"
#include "grlib/canvas.h"
#include "grlib/checkbox.h"
#include "grlib/container.h"
#include "grlib/pushbutton.h"
#include "grlib/radiobutton.h"
#include "grlib/slider.h"
#include "utils/ustdlib.h"
#include "drivers/Kentec320x240x16_ssd2119_spi.h"
#include "drivers/touch.h"
#include "images.h"

//*****************************************************************************
//
// Forward declarations for the globals required to define the widgets at
// compile-time.
//
//*****************************************************************************
void OnSettingsMain(tWidget *psWidget);
void OnGraphsMain(tWidget *psWidget);
void OnStartMotor(tWidget *psWidget);
void OnMainMenu();


// globals for buttons for settings page
void OnSettingAcceleration();
void OnSettingCurrent();
void OnSettingSpeed();

void OnIncrease();
void OnDecrease();


// globals for buttons for graphs page
void OnGraphSpeed();
void OnGraphPower();
void OnGraphAcceleration();
void OnGraphLight();

//functions
void initWidgets(tContext * sContext);
void initUI(uint32_t systemClock, tContext * Context);
void drawGraphPoint();

uint32_t g_ui32SysClock;
tContext sContext;

// Globals for the settings TODO need to set initials properly
uint32_t g_currentLimit;
uint32_t g_accelerationLimit;
uint32_t g_motorSpeed;

// Global for indicating if graph should be drawing
int g_drawingGraph;
// Global of number of points plotted
int g_numPlotPoints;
int g_numPlotOverflow;
// Global to be periodically set to update the graph
bool updateGraph;
// Global to indicate if the motor has been requested to run on UI side
bool motorStartedUI;
// Global to set the previous point
float previousPoint;


#endif /* GRLIB_DEMO_H_ */
