#include "sensor_api.h"

// constants
#define WINDOW_LIGHT        6
#define RATE_LIGHT          500 // 2Hz

// Buffer that holds the light data
float lightBuffer[WINDOW_LIGHT];

// i2c handle
I2C_Handle sensori2c;

// Setup light semaphore and SWI
Char taskLightStack[512];
Semaphore_Struct semLightStruct;
Semaphore_Handle semLightHandle;

// Setup clock SWI
Clock_Params clkSensorParams;
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

        // Variables for the ring buffer (not quite a ring buffer though)
        static uint8_t light_head = 0;
        uint16_t rawData = 0;
        float convertedLux;

        if (OPT3001ReadLight(sensori2c, &rawData)) {

            OPT3001Convert(rawData, &convertedLux);

            // system print light value
            System_printf("light: %f\n", getLight());
            System_flush();

            lightBuffer[light_head++] = convertedLux;
            light_head %= WINDOW_LIGHT;
        }
    }
}

/*
 * Initialise Semaphore for Light Task
 * */
bool initLightTask() {
    bool success = OPT3001Test(sensori2c);
    OPT3001Enable(sensori2c, true);

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
    }

    System_printf("Light setup\n");
    return success;
}

/*
 * Light Getter
 * */

float getLight() {
    float sum = 0;
    uint8_t i;

    // This is fine since window is the size of the buffer
    for (i = 0; i < WINDOW_LIGHT; i++) {
        sum += lightBuffer[i];
    }

    return (sum / (float)WINDOW_LIGHT);
}

/*
 * Create Light i2c and SWI
 * */
bool initLightSensor() {
    // I2C used by both OPT3001 and BMI160
    I2C_Params i2cParams;
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    i2cParams.transferMode = I2C_MODE_BLOCKING;
    i2cParams.transferCallbackFxn = NULL;
    sensori2c = I2C_open(Board_I2C2, &i2cParams);
    if (!sensori2c) {
        System_printf("Sensor I2C did not open\n");
        System_flush();
    }

    initLightTask();
    System_flush();

    // Used by separate init functions to create recurring SWIs. Period size is 1ms.
    Clock_Params_init(&clkSensorParams);
    clkSensorParams.startFlag = TRUE;

    // Create a recurring 2Hz SWI swiLight to post semaphore for task
    clkSensorParams.period = RATE_LIGHT;
    Clock_construct(&clockLightStruct, (Clock_FuncPtr)swiLight, 1, &clkSensorParams);

    return true;
}
