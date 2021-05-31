#include <sensors/sensors_api.h>
#include "BMI160/bmi160.h"
#include <math.h>
#include <ti/drivers/GPIO.h>

#define WINDOW_SIZE         6       // sample buffer size
#define SAMPLE_RATE         500     // sampled at 2Hz

#define ACCEL_BUFFERSIZE    5
#define TASKSTACKSIZE       2048

float lightBuffer[WINDOW_SIZE];     // buffer for light data stream

I2C_Handle light_i2c;               // i2c handle for light sensor

Char taskLightStack[512];           // light task stack size

volatile int8_t ThresholdAccel;
Semaphore_Struct semAccelStruct;
Semaphore_Handle semAccelHandle;

Semaphore_Struct semLightStruct;    // Setup for light semaphore
Semaphore_Handle semLightHandle;

Clock_Params clkSensorParams;       // Setup for clock SWI
Clock_Struct clockLightStruct;
Clock_Struct clockCurrentStruct;
Clock_Struct clockAccelerationStruct;
Char taskAccelStack[TASKSTACKSIZE];

I2C_Handle sensori2c;

float accelerationBuffer[ACCEL_BUFFERSIZE];
float accel_mag;

accel_vector av;
//UART_Handle uart;
//UART_Params uartParams;

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

// Clockswi posts semaphore at 200Hz (every 5ms)
void swiAcceleration() {
    Semaphore_post(semAccelHandle);
}

void taskAcceleration() {

    /*
     * Task will be allowed to collect data every 5ms due to
     * Clockswi posting the semaphore
     */
    int accel_pointer = 0;
    while(1) {
        // One loop = collects 1 sample data, attempts to pend again
        // but gets blocked from the sem_counter = 0, until the clockswi
        // posts the semaphore again at next clockswi period (5ms)
        Semaphore_pend(semAccelHandle, BIOS_WAIT_FOREVER);

        getAccelVector(sensori2c, &av);

//         the absolute value of the acceleration magnitude
        accel_mag = fabs(sqrt(
                pow(av.x, 2) +
                pow(av.y, 2) +
                pow(av.z, 2) ));

        // set next value in buffer to the newest sample abs magnitude
        accelerationBuffer[accel_pointer] = accel_mag;
        accel_pointer++;
        // wrap around to index first element after last element
        accel_pointer %= ACCEL_BUFFERSIZE;

//        float tempx = fabs(av.x);
//        float tempy = fabs(av.y);
//        float tempz = fabs(av.z);
//        UART_write(uart, &accel_mag, sizeof(float));
//        UART_write(uart, &tempx, sizeof(float));
//        UART_write(uart, &tempy, sizeof(float));
//        UART_write(uart, &tempz, sizeof(float));
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
 * SETUP ACCELERATION
 */
bool initAcceleration(uint16_t thresholdAccel) {
    /* Create a UART with data processing off. */
/*    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.baudRate = 115200;
    uart = UART_open(Board_UART0, &uartParams);*/

    BMI160Init(sensori2c);
    setAccelThreshold(thresholdAccel);

    // Create D4 GPIO interrupt
    GPIO_setCallback(Board_BMI160_INT, &accelEStopIntterupt);
    GPIO_enableInt(Board_BMI160_INT);

    // Create task that reads temp sensor
    // This elaborate: swi - sem - task setup
    // is needed cause the read function doesn't work in a swi
    // yet still need the recurringness a clock swi gives.
    Semaphore_Params semParams;
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&semAccelStruct, 0, &semParams);
    semAccelHandle = Semaphore_handle(&semAccelStruct);

    Task_Params taskParams;
    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.priority = 2;
    taskParams.stack = &taskAccelStack;
     Task_Handle accelTask = Task_create((Task_FuncPtr)taskAcceleration, &taskParams, NULL);
     if (accelTask == NULL) {
        System_printf("Task - ACCEL FAILED SETUP");
    }

    System_printf("Acceleration setup\n");
    return true;
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
 * GET RAW DATA AND SLIDING WINDOW FILTER
 */
volatile float accel_avg;
float getAcceleration() {
    accel_avg = 0;
    int i = 0;
    for(i = 0; i < ACCEL_BUFFERSIZE; i++){
        accel_avg += accelerationBuffer[i];
    }
    accel_avg = fabs(accel_avg/ACCEL_BUFFERSIZE);
    return accel_avg; // sumbuffer/buffersize
}

/*
 * THRESHOLD SET AND INTERRUPT TRIGGER
 */
void setAccelThreshold(uint16_t thresh) {
    ThresholdAccel = thresh;
    accelThreshold(sensori2c, thresh);
}


int triggerThreshold = 0;
void accelEStopIntterupt(unsigned int index) {
    triggerThreshold++;
    ///System_printf("THRESHOLD TRIGGER\n");
    ///System_flush();
}


/*
 * INITIALISE SENSORS
 */
bool initSensors(uint16_t thresholdAccel) {
    // INIT I2C //
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

    // INIT ACCELERATION //
    initLightTask();
    //initAcceleration(thresholdAccel);
    System_flush();

    // Used by separate init functions to create recurring SWIs. Period size is 1ms.
    Clock_Params_init(&clkSensorParams);
    clkSensorParams.startFlag = TRUE;

    // creates light SWI recurring at 2Hz
    clkSensorParams.period = SAMPLE_RATE;
    Clock_construct(&clockLightStruct, (Clock_FuncPtr)swiLight, 1, &clkSensorParams);

    return true;
}
