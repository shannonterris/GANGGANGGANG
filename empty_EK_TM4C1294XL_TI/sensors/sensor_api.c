#include "sensor_api.h"

#define WINDOW_SIZE         6       // sample buffer size
#define SAMPLE_RATE         500     // sampled at 2Hz

float lightBuffer[WINDOW_SIZE];     // buffer for light data stream

I2C_Handle light_i2c;               // i2c handle for light sensor

Char taskLightStack[512];           // light task stack size

Semaphore_Struct semLightStruct;    // Setup for light semaphore
Semaphore_Handle semLightHandle;

Clock_Params clkSensorParams;       // Setup for clock SWI
Clock_Struct clockLightStruct;

/*
 * Light SWI
 * */
void swiLight() {
    Semaphore_post(semLightHandle);
}

/*
 * Light Task
 * */
void taskLight() {
    while(1) {
        Semaphore_pend(semLightHandle, BIOS_WAIT_FOREVER);

        // ring buffer to handle stream of light data with 6 samples
        uint16_t lightData = 0;
        float convertedLux;
        static uint8_t bufferHead = 0;

        if (OPT3001Read(light_i2c, &lightData)) {

            OPT3001Convert(lightData, &convertedLux);

            lightBuffer[bufferHead++] = convertedLux;   // add lux val to position of buffer head
            bufferHead %= WINDOW_SIZE;                  // resets buffer head when > 7
        }
    }
}

/*
 * Initialise Semaphore for Light Task
 * */
bool initLightTask() {
    bool success = OPT3001Test(light_i2c);
    OPT3001Enable(light_i2c, true);

    Semaphore_Params semParams;
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&semLightStruct, 0, &semParams);
    semLightHandle = Semaphore_handle(&semLightStruct);

    Task_Params taskParams;
    Task_Params_init(&taskParams);
    taskParams.stackSize = 512;
    taskParams.priority = 2;
    taskParams.stack = &taskLightStack;
    Task_Handle lightTask = Task_create((Task_FuncPtr)taskLight, &taskParams, NULL);
    if (lightTask == NULL) {
        System_printf("Task - LIGHT FAILED SETUP");
        System_flush();
    }

    System_printf("Light successfully setup\n");
    return success;
}

/*
 * Light Getter - filters for average light over the 6 samples
 * */
float getLight() {
    float lightSum = 0;
    uint8_t i;

    for (i = 0; i < WINDOW_SIZE; i++) {
        lightSum += lightBuffer[i];
    }

    return (lightSum / (float)WINDOW_SIZE);
}

/*
 * Create Light i2c and SWI
 * */
bool initLightSensor() {

    I2C_Params i2cParams;
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    i2cParams.transferMode = I2C_MODE_BLOCKING;
    i2cParams.transferCallbackFxn = NULL;
    light_i2c = I2C_open(Board_I2C2, &i2cParams);
    if (!light_i2c) {
        System_printf("Sensor I2C did not open\n");
        System_flush();
    }

    initLightTask();
    System_flush();

    // clock to create SWI, period is 1ms
    Clock_Params_init(&clkSensorParams);
    clkSensorParams.startFlag = TRUE;

    // creates light SWI recurring at 2Hz
    clkSensorParams.period = SAMPLE_RATE;
    Clock_construct(&clockLightStruct, (Clock_FuncPtr)swiLight, 1, &clkSensorParams);

    return true;
}
