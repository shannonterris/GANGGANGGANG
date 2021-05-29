#include "sensors_api.h"
#include "BMI160/bmi160.h"
#include <math.h>
#include <ti/drivers/GPIO.h>

#define ACCEL_BUFFERSIZE    5
#define TASKSTACKSIZE       512

volatile int8_t ThresholdAccel;
float accel_mag;
Semaphore_Struct semAccelStruct;
Semaphore_Handle semAccelHandle;

Clock_Params clkSensorParams;
Clock_Struct clockCurrentStruct;
Clock_Struct clockAccelerationStruct;
Char taskAccelStack[512];
I2C_Handle sensori2c;
float accelerationBuffer[ACCEL_BUFFERSIZE];
accel_vector av;
UART_Handle uart;
UART_Params uartParams;
/*
 * THREAD FUNCTIONS
 */
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

        // the absolute value of the acceleration magnitude
        accel_mag = fabs(sqrt(
                pow(av.x, 2) +
                pow(av.y, 2) +
                pow(av.z, 2) ));

        // set next value in buffer to the newest sample abs magnitude
        accelerationBuffer[accel_pointer] = accel_mag;
        accel_pointer++;
        // wrap around to index first element after last element
        accel_pointer %= ACCEL_BUFFERSIZE;

        float tempx = fabs(av.x);
        float tempy = fabs(av.y);
        float tempz = fabs(av.z);
        UART_write(uart, &accel_mag, sizeof(float));
        UART_write(uart, &tempx, sizeof(float));
        UART_write(uart, &tempy, sizeof(float));
        UART_write(uart, &tempz, sizeof(float));
    }
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
    System_printf("THRESHOLD TRIGGER\n");
    System_Flush();
}

/*
 * SETUP ACCELERATION
 */
bool initAcceleration(uint16_t thresholdAccel) {
    /* Create a UART with data processing off. */
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.baudRate = 115200;
    uart = UART_open(Board_UART0, &uartParams);



    BMI160Init(sensori2c);
    setAccelThreshold(thresholdAccel);

    // Create D4 GPIO interrupt
    GPIO_setCallback(Board_BMI160_INT, accelEStopIntterupt);
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
    taskParams.priority = 11;
    taskParams.stack = &taskAccelStack;
    Task_Handle accelTask = Task_create((Task_FuncPtr)taskAcceleration, &taskParams, NULL);
    if (accelTask == NULL) {
        System_printf("Task - ACCEL FAILED SETUP");
    }

    System_printf("Acceleration setup\n");
    return true;
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
    // i2cParams.transferCallbackFxn = NULL;
    sensori2c = I2C_open(Board_I2C2, &i2cParams);
    if (sensori2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }
    else {
        System_printf("I2C Initialized!\n");
    }

    // INIT ACCELERATION //
    initAcceleration(thresholdAccel);
    System_flush();

    // Used by separate init functions to create recurring SWIs. Period size is 1ms.
    Clock_Params_init(&clkSensorParams);
    clkSensorParams.startFlag = TRUE;

    // Create a recurring 200Hz SWI swiAcceleration to post semaphore for task to perform 1 instance of collecting data
    clkSensorParams.period = 5; // 5ms = 200Hz swiAcceleration
    Clock_construct(&clockAccelerationStruct, (Clock_FuncPtr) swiAcceleration, 1, &clkSensorParams);

    return true;
}
