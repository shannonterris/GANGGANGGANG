//*****************************************************************************
//
// fonttest.c - Simple font testcase.
//
// Copyright (c) 2013-2017 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.4.178 of the EK-TM4C1294XL Firmware Package.
//
//*****************************************************************************
#include "grlib_demo.h"
#include "motor/MotorCode.h"

//
//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif


//*****************************************************************************
//
// The DMA control structure table.
//
//*****************************************************************************
#ifdef ewarm
#pragma data_alignment=1024
tDMAControlTable psDMAControlTable[64];
#elif defined(ccs)
#pragma DATA_ALIGN(psDMAControlTable, 1024)
tDMAControlTable psDMAControlTable[64];
#else
tDMAControlTable psDMAControlTable[64] __attribute__ ((aligned(1024)));
#endif


// Macros for different setting / graph pages
# define ACCELERATION 1
# define SPEED 2
# define CURRENT 3
# define POWER 4
# define LIGHT 5

// Graph bounds
#define X_GRAPH 0
#define Y_GRAPH 35
#define WIDTH_GRAPH 320
#define HEIGHT_GRAPH 135
#define MAX_PLOT_SAMPLES 300 // If sampling at 10Hz (max of 30sec)

int graphIntMax;
int graphIntMin;

int32_t x_graph_step = 1;
int32_t x_graph_start = 10;
float y_graph_step;
int32_t y_graph_start = 45;
int32_t y_graph_max = 50;
int32_t y_graph_end = 180;

//*****************************************************************************
//
// Canvas for main menus
//
//*****************************************************************************
Canvas(g_sMainPage, WIDGET_ROOT, 0, 0, &g_sKentec320x240x16_SSD2119, 0, 0,
       320, 320, CANVAS_STYLE_FILL, ClrBlack, 0, 0, 0, 0, 0, 0);
Canvas(g_sSettingsMainPage, &g_sMainPage, 0, 0, &g_sKentec320x240x16_SSD2119, 0, 0,
       320, 320, CANVAS_STYLE_FILL, ClrBlack, 0, 0, 0, 0, 0, 0);
Canvas(g_sGraphsMainPage, &g_sMainPage, 0, 0, &g_sKentec320x240x16_SSD2119, 0, 0,
       320, 320, CANVAS_STYLE_FILL, ClrBlack, 0, 0, 0, 0, 0, 0);

//*****************************************************************************
//
// Canvas for settings pages and graphs pages
//
//*****************************************************************************
Canvas(g_sSettingPage, &g_sSettingsMainPage, 0, 0, &g_sKentec320x240x16_SSD2119, 0, 0,
       320, 320, CANVAS_STYLE_FILL, ClrBlack, 0, 0, 0, 0, 0, 0);
Canvas(g_sGraphPage, &g_sGraphsMainPage, 0, 0, &g_sKentec320x240x16_SSD2119, 0, 0,
       320, 320, CANVAS_STYLE_FILL, ClrBlack, 0, 0, 0, 0, 0, 0);


//*****************************************************************************
//
// Widgets required for the main menu
//
//*****************************************************************************
RectangularButton(g_sSetOption, &g_sMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 25, 145, 150,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPeachPuff, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Settings", 0, 0, 0, 0, OnSettingsMain);
RectangularButton(g_sGraphOption, &g_sMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 165, 25, 145, 150,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPeachPuff, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Graphs", 0, 0, 0, 0, OnGraphsMain);
RectangularButton(g_sMotorOption, &g_sMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 180, 300, 50,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPowderBlue, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Start Motor", 0, 0, 0, 0, OnStartMotor);

//*****************************************************************************
//
// Widgets required for the settings menu screen
//
//*****************************************************************************

RectangularButton(g_sSettingAcceleration, &g_sSettingsMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 30, 300, 45,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPeachPuff, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Acceleration", 0, 0, 0, 0, OnSettingAcceleration);
RectangularButton(g_sSettingSpeed, &g_sSettingsMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 80, 300, 45,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPeachPuff, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Speed", 0, 0, 0, 0, OnSettingSpeed);
RectangularButton(g_sSettingCurrent, &g_sSettingsMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 130, 300, 45,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPeachPuff, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Current", 0, 0, 0, 0, OnSettingCurrent);
RectangularButton(g_sSettingBack, &g_sSettingsMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 180, 300, 50,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPowderBlue, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Back", 0, 0, 0, 0, OnMainMenu);


//*****************************************************************************
//
// Widgets required for the graphs menu screen
//
//*****************************************************************************

 RectangularButton(g_sGraphAcceleration, &g_sSettingsMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 25, 150, 70,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPeachPuff, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Acceleration", 0, 0, 0, 0, OnGraphAcceleration);
RectangularButton(g_sGraphSpeed, &g_sSettingsMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 165, 25, 150, 70,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPeachPuff, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Speed", 0, 0, 0, 0, OnGraphSpeed);
RectangularButton(g_sGraphPower, &g_sSettingsMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 100, 150, 70,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPeachPuff, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Power", 0, 0, 0, 0, OnGraphPower);
RectangularButton(g_sGraphLight, &g_sSettingsMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 165, 100, 150, 70,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPeachPuff, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Light", 0, 0, 0, 0, OnGraphLight);
RectangularButton(g_sGraphBack, &g_sSettingsMainPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 180, 300, 50,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPowderBlue, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Back", 0, 0, 0, 0, OnMainMenu);

//*****************************************************************************
//
// Widgets required for setting screen
//
//*****************************************************************************
RectangularButton(g_sSettingIncrease, &g_sSettingPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 220, 80, 90, 70,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrGreen, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "+", 0, 0, 0, 0, OnIncrease);
RectangularButton(g_sSettingDecrease, &g_sSettingPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 80, 90, 70,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrRed, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "-", 0, 0, 0, 0, OnDecrease);
RectangularButton(g_sSettingValue, &g_sSettingPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 105, 80, 110, 70,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL),
                    ClrBlack, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Current Value (UNITS)", 0, 0, 0, 0, 0);
RectangularButton(g_sSettingTitle, &g_sSettingPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 40, 40, 250, 25,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL),
                    ClrBlack, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Setting (units): ", 0, 0, 0, 0, 0);
RectangularButton(g_sSettingExit, &g_sSettingPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 180, 300, 50,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPowderBlue, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Exit", 0, 0, 0, 0, OnSettingsMain);

//*****************************************************************************
//
// Widgets required for graphing page
//
//*****************************************************************************
Canvas(g_sGraph, &g_sGraphPage, 0, 0,
       &g_sKentec320x240x16_SSD2119, X_GRAPH, Y_GRAPH, WIDTH_GRAPH, HEIGHT_GRAPH,
       CANVAS_STYLE_FILL, ClrBlack, 0, 0, 0, 0, 0, 0);
RectangularButton(g_sGraphTitle, &g_sGraphPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 5, 170, 20,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL),
                    ClrBlack, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Graphing", 0, 0, 0, 0, 0);
RectangularButton(g_sGraphExit, &g_sGraphPage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 10, 200, 300, 30,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                    ClrPowderBlue, ClrBlack, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Exit", 0, 0, 0, 0, OnGraphsMain);




//*****************************************************************************
//
// The panel that is currently being displayed for graphing or setting
//
//*****************************************************************************
uint32_t g_sCurrentPanel;

//*****************************************************************************
//
// Handles presses of the settings panel button.
//
//*****************************************************************************
void
OnMainMenu(tWidget *psWidget)
{

    //
    // Remove the current possible panels.
    //
    WidgetRemove((tWidget *) &g_sSettingsMainPage);
    WidgetRemove((tWidget *) &g_sGraphsMainPage);

    //
    // Add and draw the new panel.
    //
    WidgetPaint((tWidget *) &g_sMainPage);
    WidgetAdd(WIDGET_ROOT, (tWidget *) &g_sMainPage);
}


//*****************************************************************************
//
// Handles presses of the settings panel button.
//
//*****************************************************************************
void
OnSettingsMain(tWidget *psWidget)
{

    //
    // Remove the current panel.
    //
    WidgetRemove((tWidget *) &g_sMainPage);
    WidgetRemove((tWidget *) &g_sSettingPage);

    //
    // Add and draw the new panel.
    //
    WidgetPaint((tWidget *) &g_sSettingsMainPage);
    WidgetAdd(WIDGET_ROOT, (tWidget *) &g_sSettingsMainPage);
}

//*****************************************************************************
//
// Handles presses of the graphs panel button.
//
//*****************************************************************************
void
OnGraphsMain(tWidget *psWidget)
{

    //
    // Remove the current panel.
    //
    WidgetRemove((tWidget *) &g_sMainPage);
    WidgetRemove((tWidget *) &g_sGraphPage);

    // Stop Drawing Graph / Reset Graphing Variables
    if (g_drawingGraph) {
        g_drawingGraph = 0;
        g_numPlotPoints = 0;
        g_numPlotOverflow = 1;
    }

    //
    // Add and draw the new panel.
    //
    WidgetPaint((tWidget *) &g_sGraphsMainPage);
    WidgetAdd(WIDGET_ROOT, (tWidget *) &g_sGraphsMainPage);

}

//*****************************************************************************
//
// Handles presses of the start motor.
//
//*****************************************************************************
void
OnStartMotor(tWidget *psWidget)
{
    motorStartedUI = !motorStartedUI;

    startMotor(motorStartedUI);

    if(motorStartedUI)
    {
        PushButtonTextSet(&g_sMotorOption, "Stop Motor");
        WidgetPaint((tWidget *)&g_sMotorOption);
    }
    else
    {
        PushButtonTextSet(&g_sMotorOption, "Start Motor");
        WidgetPaint((tWidget *)&g_sMotorOption);
    }
}


//*****************************************************************************
// Function to create generic settings page
//*****************************************************************************

char valueText[20];
void drawSettingValue(char * units, int value) {
    // Create single string of the units and value
    sprintf(valueText, "%d %s", value, units);
    PushButtonTextSet((tPushButtonWidget *)&g_sSettingValue, valueText);
    WidgetPaint((tWidget *) &g_sSettingValue);
}


void OnSettingsPage() {

    // Remove Settings Main Menu
    WidgetRemove((tWidget *) &g_sSettingsMainPage);

    //
    // Add and draw the new panel.
    //
    WidgetPaint((tWidget *) &g_sSettingPage);
    WidgetAdd(WIDGET_ROOT, (tWidget *) &g_sSettingPage);

    // Draw appropriate title
    if (g_sCurrentPanel == ACCELERATION) {
        PushButtonTextSet((tPushButtonWidget *)&g_sSettingTitle, "Set Acceleration Upper Limit");
        drawSettingValue("m/s^2", g_accelerationLimit);
    }

    else if (g_sCurrentPanel == SPEED) {
        PushButtonTextSet((tPushButtonWidget *)&g_sSettingTitle, "Set Motor Speed");
        drawSettingValue("RPM", g_motorSpeed);
    }

    else if (g_sCurrentPanel == CURRENT) {
        PushButtonTextSet((tPushButtonWidget *)&g_sSettingTitle, "Set Current Upper Limit");
        drawSettingValue("mA", g_currentLimit);
    }
    WidgetPaint((tWidget *) &g_sSettingTitle);
}


void OnIncrease() {
    // TODO find appropriate amounts to go up in
    if (g_sCurrentPanel == ACCELERATION) {
        // Acceleration increase in units of 1
        if (g_accelerationLimit <= 39) {  // Acceleration has a max limit of 40
            g_accelerationLimit++;
            drawSettingValue("m/s^2", g_accelerationLimit);
        }
        // TODO api call to set new value
    }

    else if (g_sCurrentPanel == SPEED) {
        // Motor speed increase in units of 100
        if (getState() == true) {
            if (g_motorSpeed <= 4400) { // Speed has a max limit of 4500
                g_motorSpeed = g_motorSpeed + 100;
                drawSettingValue("RPM", g_motorSpeed);
                setSpeed(g_motorSpeed);
            }
        } else {
            g_motorSpeed = 0;
            drawSettingValue("RPM", g_motorSpeed);
            setSpeed(g_motorSpeed);
        }
        // TODO api call to set new value
    }

    else if (g_sCurrentPanel == CURRENT) {
        g_currentLimit++;
        drawSettingValue("mA", g_currentLimit);
        // TODO api call to set new value
    }
}

void OnDecrease() {
    // TODO find appropriate amounts to go up in
    if (g_sCurrentPanel == ACCELERATION) {
        // Acceleration decrease in units of 1
        if (g_accelerationLimit >= 11) { // Acceleration has a minimum of 10
            g_accelerationLimit--;
            drawSettingValue("m/s^2", g_accelerationLimit);
        }
        // TODO api call to set new value
    }

    else if (g_sCurrentPanel == SPEED) {
        // Motor speed increase in units of 100
        if (getState() == true) {
            if (g_motorSpeed >= 100) {
                g_motorSpeed = g_motorSpeed - 100;
                drawSettingValue("RPM", g_motorSpeed);
                setSpeed(g_motorSpeed);
            }
        } else {
            g_motorSpeed = 0;
            drawSettingValue("RPM", g_motorSpeed);
            setSpeed(g_motorSpeed);
        }
        // TODO api call to set new value
    }

    else if (g_sCurrentPanel == CURRENT) {
        g_currentLimit--;
        drawSettingValue("mA", g_currentLimit);
        // TODO api call to set new value
    }
}

//*****************************************************************************
// Drawing for Settings Page
//*****************************************************************************
void OnSettingAcceleration() {
    g_sCurrentPanel = ACCELERATION;
    OnSettingsPage();

}

void OnSettingSpeed() {
    g_sCurrentPanel = SPEED;
    OnSettingsPage();

}

void OnSettingCurrent() {
    g_sCurrentPanel = CURRENT;
    OnSettingsPage();
}

//*****************************************************************************
// Function to update graph
//*****************************************************************************

char graphMin[5];
char graphMax[5];
// TODO remove just for testing
float float_rand( float min, float max )
{
    float scale = rand() / (float) RAND_MAX; /* [0, 1.0] */
    return min + scale * ( max - min );      /* [min, max] */
}

char maxTime[30];
char minTime[30];
void drawGraphPoint() {
    // Reset graph when maximum plot samples has been reached
    if (g_numPlotPoints == MAX_PLOT_SAMPLES) {
        g_numPlotPoints = 0;
        // Redraw Graph
        WidgetPaint((tWidget *) &g_sGraph);
        WidgetMessageQueueProcess();

        // Draw Graph
        GrStringDraw(&sContext, "Time", -1, 140, 181, false);
        GrLineDrawV(&sContext, 10, 45, 180);
        GrLineDrawH(&sContext, 10, 305, 180);
        GrLineDraw(&sContext, 10, 45, 8,50);
        GrLineDraw(&sContext, 10, 45, 12, 50);
        GrLineDraw(&sContext, 305, 180, 303, 178);
        GrLineDraw(&sContext, 305, 180, 303, 182);

        GrStringDraw(&sContext, minTime, -1, 10, 181, false);
        GrStringDraw(&sContext, graphMax, -1, 5, 30, false);
        sprintf(minTime, "%ds", g_numPlotOverflow*30); // Times by 30 as the overflow occurs at 30 seconds
        g_numPlotOverflow++;
        sprintf(maxTime, "%ds", g_numPlotOverflow*30); // Times by 30 as the overflow occurs at 30 seconds

        GrStringDraw(&sContext, "           ", -1, 10, 181, true);
        GrStringDraw(&sContext, "           ", -1, 290, 181, true);
        GrStringDraw(&sContext, minTime, -1, 10, 181, false);
        GrStringDraw(&sContext, maxTime, -1, 290, 181, false);
        GrFlush(&sContext);
        return;
    }

    // somehow need to get the currentPoint to plot
    // TODO using random float for testing remove later on
    //float currentPoint = float_rand(graphIntMin, graphIntMax);
      float currentPoint = (float) getSpeed();
    // graphMax - graphMin is the space between
    // the physical space between is 135
    // graphMin should be 10 and graphMax should be 45
    int32_t x1 = (g_numPlotPoints * x_graph_step) + x_graph_start;
    int32_t x2 = (g_numPlotPoints + 1) * x_graph_step + x_graph_start;
    int32_t y1 = (y_graph_start + HEIGHT_GRAPH) - (previousPoint - graphIntMin)*y_graph_step;
    int32_t y2 = (y_graph_start + HEIGHT_GRAPH) - (currentPoint - graphIntMin)*y_graph_step;

    // Draw Current Value
    static char currentValue[30];
    sprintf(currentValue, "%5.2f", currentPoint);
    GrStringDrawCentered(&sContext, "Value:", -1, 210, 15, true);
    GrStringDrawCentered(&sContext, "          ", -1, 270, 15, true);
    GrStringDrawCentered(&sContext, currentValue, -1, 270, 15, true);
    // Draw line
    GrLineDraw(&sContext, x1, y1, x2, y2);
    g_numPlotPoints++;

    // Save position of previous sample
    previousPoint = currentPoint;
    // Set to false so you can continue to process the exit button
    updateGraph = false;

}

//*****************************************************************************
// Function to create generic graphs page
//*****************************************************************************

void OnGraphsPage(char * title, int yMin, int yMax) {
    // Remove Graph Main Menu
    WidgetRemove((tWidget *) &g_sGraphsMainPage);
    //
    // Add and draw the new panel. Draw the title of the graph
    //
    WidgetPaint((tWidget *) &g_sGraphPage);
    WidgetAdd(WIDGET_ROOT, (tWidget *) &g_sGraphPage);
    PushButtonTextSet((tPushButtonWidget *)&g_sGraphTitle, title);
    WidgetMessageQueueProcess();

    graphIntMax = yMax;
    graphIntMin = yMin;

    // Draw the y-axis limits
    sprintf(graphMin, "%d", yMin);
    sprintf(graphMax, "%d", yMax);
    // Set step scale for y-axis
    y_graph_step = (float)HEIGHT_GRAPH/(float)(graphIntMax - graphIntMin);

    // Draw the graph
    GrStringDraw(&sContext, "Time", -1, 140, 181, false);
    GrLineDrawV(&sContext, 10, 45, 180);
    GrLineDrawH(&sContext, 10, 305, 180);
    GrLineDraw(&sContext, 10, 45, 8,50);
    GrLineDraw(&sContext, 10, 45, 12, 50);
    GrLineDraw(&sContext, 305, 180, 303, 178);
    GrLineDraw(&sContext, 305, 180, 303, 182);
    GrStringDraw(&sContext, graphMax, -1, 5, 30, false);

    GrStringDraw(&sContext, "0s", -1, 10, 181, false);
    GrStringDraw(&sContext, "30s", -1, 290, 181, false);
    // Set global pointer to data TODO
    data_Graph = 10.000;
    previousPoint = 0;
    g_numPlotOverflow = 1;

    // Set global to start drawing graph
    g_drawingGraph = 1;
    while (g_drawingGraph) {
        if(updateGraph) { // Update graph at 10Hz
            drawGraphPoint();
        }
        WidgetMessageQueueProcess();
    }
}

void OnGraphSpeed() {
    g_sCurrentPanel = SPEED;
    OnGraphsPage("Speed (RPM)", 0, 5000);

}
void OnGraphPower(){
    g_sCurrentPanel = POWER;
    OnGraphsPage("Power (W)", 0, 100); // TODO


}
void OnGraphAcceleration(){
    g_sCurrentPanel = ACCELERATION;
    OnGraphsPage("Acceleration (m/s^2)", 0, 50);

}
void OnGraphLight(){
    g_sCurrentPanel = LIGHT;
    OnGraphsPage("Light (Lux)", 0, 100); //TODO

}

void initUI(uint32_t systemClock, tContext * Context) {
    //
    // Initialize the display driver.
    //
    Kentec320x240x16_SSD2119Init(systemClock);

    //
    // Initialize the graphics context.
    //
    GrContextInit(Context, &g_sKentec320x240x16_SSD2119);

    //
    // Initialize the Touch Screen
    //
    TouchScreenInit(systemClock);
    TouchScreenCallbackSet(WidgetPointerMessage);

    // Init limits
    g_currentLimit = 0;
    g_accelerationLimit = 10;
    g_motorSpeed = 0;

    g_drawingGraph = 0;
    // Global of number of points plotted
    g_numPlotPoints = 0;
    g_numPlotOverflow = 0;
    // Global to be periodically set to update the graph
    updateGraph = false;
    // Start motor on start up
    motorStartedUI = false;
    previousPoint = 0;
    // currentPoint = 0;
}

//*****************************************************************************
//
// Function to initialize widgets for each page
//
//*****************************************************************************
void initWidgets(tContext * sContext) {
    //
    // Initialize the font and foreground colors
    //
    GrContextForegroundSet(sContext, ClrWhite);
    GrContextFontSet(sContext, &g_sFontCmss18);

    //
    // MAIN MENU - Add the 3 widgets required for the main menu.
    //
    WidgetAdd(WIDGET_ROOT, (tWidget *)&g_sMainPage);
    WidgetAdd((tWidget *)&g_sMainPage, (tWidget *)&g_sSetOption);
    WidgetAdd((tWidget *)&g_sMainPage, (tWidget *)&g_sGraphOption);
    WidgetAdd((tWidget *)&g_sMainPage, (tWidget *)&g_sMotorOption);

    //
    // SETTING MENU - Add the widgets required for the setting menu.
    //
    WidgetAdd((tWidget *)&g_sSettingsMainPage, (tWidget *)&g_sSettingAcceleration);
    WidgetAdd((tWidget *)&g_sSettingsMainPage, (tWidget *)&g_sSettingSpeed);
    WidgetAdd((tWidget *)&g_sSettingsMainPage, (tWidget *)&g_sSettingCurrent);
    WidgetAdd((tWidget *)&g_sSettingsMainPage, (tWidget *)&g_sSettingBack);

    // SETTING PAGE - Add the widgets required for a settings page
    WidgetAdd((tWidget *)&g_sSettingPage, (tWidget *) &g_sSettingIncrease);
    WidgetAdd((tWidget *)&g_sSettingPage, (tWidget *) &g_sSettingDecrease);
    WidgetAdd((tWidget *)&g_sSettingPage, (tWidget *) &g_sSettingExit);
    WidgetAdd((tWidget *)&g_sSettingPage, (tWidget *) &g_sSettingTitle);
    WidgetAdd((tWidget *)&g_sSettingPage, (tWidget *) &g_sSettingValue);

    //
    // GRAPH MENU - Add the widgets required for the graph menu.
    //
    WidgetAdd((tWidget *)&g_sGraphsMainPage, (tWidget *)&g_sGraphAcceleration);
    WidgetAdd((tWidget *)&g_sGraphsMainPage, (tWidget *)&g_sGraphSpeed);
    WidgetAdd((tWidget *)&g_sGraphsMainPage, (tWidget *)&g_sGraphPower);
    WidgetAdd((tWidget *)&g_sGraphsMainPage, (tWidget *)&g_sGraphLight);
    WidgetAdd((tWidget *)&g_sGraphsMainPage, (tWidget *)&g_sGraphBack);

    // GRAPH PAGE - Add the widgets required for the graph page.
    WidgetAdd((tWidget *)&g_sGraphPage, (tWidget *)&g_sGraphExit);
    WidgetAdd((tWidget *)&g_sGraphPage, (tWidget *)&g_sGraph);
    WidgetAdd((tWidget *)&g_sGraphPage, (tWidget *)&g_sGraphTitle);

    //
    // Issue the initial paint request to the widgets.
    //
    WidgetPaint(WIDGET_ROOT);
}


//*****************************************************************************
//
// A simple demonstration of the features of the TivaWare Graphics Library.
//
//*****************************************************************************

