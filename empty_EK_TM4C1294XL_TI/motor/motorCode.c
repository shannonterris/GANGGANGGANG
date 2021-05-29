/*
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== empty.c ========
 */
/* XDCtools Header files */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Types.h>
#include "motorlib.h"

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/gates/GateHwi.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/Knl/Clock.h>

#include <ti/sysbios/knl/Swi.h>

/* Include a mailbox for the buffer */
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/Hal/Timer.h>
#include <ti/sysbios/knl/Queue.h>

/* TI-RTOS Header files */
// #include <ti/drivers/EMAC.h>
#include <ti/drivers/GPIO.h>
// #include <ti/drivers/I2C.h>
// #include <ti/drivers/SDSPI.h>
// #include <ti/drivers/SPI.h>
#include <ti/drivers/UART.h>
// #include <ti/drivers/USBMSCHFatFs.h>
// #include <ti/drivers/Watchdog.h>
// #include <ti/drivers/WiFi.h>

#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/adc.h"

/* Board Header file */
#include "Board.h"
#include "driverlib/interrupt.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/gpio.h"

#define TASKSTACKSIZE   2048
#define TASK2STACK      512
#define TOTAL_POSITIONS 12
#define SAMPLE_SIZE 100
#define ACCEL_SIZE 100
#define MAXOUTPUT 24

#define NUMSGS 50

typedef struct MsgObj {
    Queue_Elem elem; // first field
    //Int id; // writer task ID
    float rpm; // message value (rpm_actual)
} MsgObj;

MsgObj msg_mem[NUMSGS]; // allocate initial message memory

Queue_Handle queue;

//Mailbox_Struct mbxStruct;
//Mailbox_Handle mbxHandle;
Semaphore_Struct semStruct;
Semaphore_Handle semHandle;


Clock_Struct clkStruct;
Clock_Handle clkHandle;

Hwi_Handle hallA, hallB, hallC;

Hwi_Handle adc0, adc1;

Error_Block eb;
Event_Struct evtStruct;
Event_Handle evtHandle;
//Error_init(&eb);

UInt gateKey;
GateHwi_Handle gateHwi;
GateHwi_Params gHwiprms;

Timer_Handle myTimer;
Timer_Params timerParams;
Task_Struct task0Struct;
Char task0Stack[TASKSTACKSIZE];
Char task1Stack[TASK2STACK];

Swi_Handle SwiHandle;



uint32_t g_ui32SysClock;

UART_Handle uart;

bool success, print_from_HWI, print_from_motor, print_from_clock;

volatile int motor = 0;
volatile int prevMotor = 0;
volatile int prevSysTick = 0;
volatile int delta_ticks, SysTicks;
volatile int deltaPos = 0;
volatile int current_motor;
//volatile float clock_time = 0.00416667;
//volatile float clock_time_mins = 0.000333333;
volatile float clock_time_mins = 0.00016667;
//volatile float clock_time_mins = 0.00016667/2;
volatile float clock_time_secs = 0.01;
//volatile float clock_time = 0.00166667;

volatile float rpm_actual = 0;
volatile float delta_time;
volatile float delta_rpm;
volatile float acceleration;
volatile float acceleration_avg;
volatile float acceleration_sum = 0;

volatile rpm_prev = 0;

volatile int error;
volatile float integral_error = 0;

volatile float rpm_desired = 800;
volatile float rpm_int = 0;
volatile float jump = 5;

//volatile float Kp = 0.5;
//volatile float Ki = 0.02;
volatile float Kp = 0.00085;
volatile float Ki = 0.00005;

volatile float output;

volatile int clock_count = 0;
volatile float rpm_sum = 0;
volatile float rpm_avg;

unsigned int hA, hB, hC;
unsigned int hA_init, hB_init, hC_init;

float min_per_sysTick = 0.000017;
volatile float rotation = (float) 1/TOTAL_POSITIONS;

volatile int deltaPos_c;
volatile float rotation_c, rpm_actual_c, rpm_avg_c;
volatile float rpm_sum_c = 0;

volatile float rpm_buf[5];

volatile bool motorRunning;

volatile int dummy = 0;


/**
 * !! HWI THREAD !!
 * Read hall-effect sensor lines and
 * update the motor
 */
void  HallFxn()
{
    SysTicks = Clock_getTicks();
    //GPIO_disableInt(Board_LED0);
    /* Clear all interrupt sources */
    GPIO_clearInt(HALL_A);
    GPIO_clearInt(HALL_B);
    GPIO_clearInt(HALL_C);

    /* read in hall-effect values */
    hA = GPIO_read(HALL_A);
    hB = GPIO_read(HALL_B);
    hC = GPIO_read(HALL_C);

    //gateKey = GateHwi_enter(gateHwi);
    motor++; // increment positional counter
    //GateHwi_leave(gateHwi, gateKey);

    // push motor again
    updateMotor(hA, hB, hC);

    /* feed values into updateMotor to drive the motor */
    //SysTicks = Clock_getTicks();
    delta_time = (min_per_sysTick*(SysTicks - prevSysTick)); // delta time is change in ticks * time (mins) for one tick
    rpm_actual = rotation/delta_time;
    prevSysTick = SysTicks;
    //print_from_motor = true;
}

void ADCFxn()
{
    GPIO_clearInt(ADC_0);
    dummy++;
}

/**
 * Setup GPIO interrupts (HWI) on hall effect sensor lines
 */
void initHallABC()
{
    // Hwi params should be the same for each HWI
    Hwi_Params hwiParams;
    Hwi_Params_init(&hwiParams);

    /* each handle has the same call-back function -> HallFxn */
    // ISR vector number 88 for port M -> Hall_A is port M pin 3
    hallA = Hwi_create(INT_GPIOM_TM4C129, (Hwi_FuncPtr)HallFxn, &hwiParams, NULL);
    // ISR vector number 48 for port H -> Hall_B is port H pin 2
    hallB = Hwi_create(INT_GPIOH_TM4C123, (Hwi_FuncPtr)HallFxn, &hwiParams, NULL);
    // ISR vector number 89 for port N -> Hall_C is port N pin 2
    hallC = Hwi_create(INT_GPION_TM4C129, (Hwi_FuncPtr)HallFxn, &hwiParams, NULL);

    if(hallA == NULL || hallB == NULL || hallC == NULL)
    {
        System_abort("Hall HWI creation failed...");
    }



    /* GPIO_enableInt(Board_LED0); */
    /* Enable GPIO interrupts for each GPIO line */
    // HALL_A/B/C traced to EK_TM4C1294XL.h file, GPIO array
    GPIO_enableInt(HALL_A);
    GPIO_enableInt(HALL_B);
    GPIO_enableInt(HALL_C);

    GPIOIntTypeSet(GPIO_PORTM_BASE, GPIO_PIN_3, GPIO_RISING_EDGE);
    GPIOIntTypeSet(GPIO_PORTH_BASE, GPIO_PIN_2, GPIO_RISING_EDGE);
    GPIOIntTypeSet(GPIO_PORTN_BASE, GPIO_PIN_2, GPIO_RISING_EDGE);

    GPIODirModeSet(GPIO_PORTM_BASE, GPIO_PIN_3, GPIO_DIR_MODE_IN);
    GPIODirModeSet(GPIO_PORTH_BASE, GPIO_PIN_2, GPIO_DIR_MODE_IN);
    GPIODirModeSet(GPIO_PORTN_BASE, GPIO_PIN_2, GPIO_DIR_MODE_IN);

    //------- ADC! //
    // ISR vector number 20 for Port E -> AIN0 (channel 0) is port E3
    adc0 = Hwi_create(INT_GPIOE_TM4C123, (Hwi_FuncPtr)ADCFxn, &hwiParams, NULL);

    if (adc0 == NULL)
    {
        System_abort("ADC HWI creation failed...");
    }

    GPIO_enableInt(ADC_0);
    GPIOIntTypeSet(GPIO_PORTE_BASE, GPIO_PIN_3, GPIO_RISING_EDGE);
    GPIODirModeSet(GPIO_PORTE_BASE, GPIO_PIN_3, GPIO_DIR_MODE_IN);

    System_printf("Done initHallABC\n");
    System_flush();
    //Board_BUTTON0  User switch 1
}

/*
 * Reads the GPIO lines - positions of the hall effect sensors
 */
void readABC()
{
    hA = GPIO_read(HALL_A);
    hB = GPIO_read(HALL_B);
    hC = GPIO_read(HALL_C);
}


UInt32 time;



/*
 * !! TASK THREAD !!
 * Sets up UART for testing and printing purposes
 * initialises the motor library, sets initial PWM,
 * reads initial GPIO lines and updates the motor once
 * sets up HWI
 */
void motorFxn(UArg arg0, UArg arg1)
{

    // -------------------------------------------

    /* UART used for printing purposes */
    UART_Params uartParams;
    UART_Params_init(&uartParams);
    uartParams.readTimeout = BIOS_WAIT_FOREVER;
    uartParams.writeTimeout = BIOS_WAIT_FOREVER;
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.baudRate = 115200;

    uart = UART_open(Board_UART0, &uartParams);

    if (uart == NULL)
    {
        // test UART
        System_abort("Error opening the UART");
    }

    //char input[200]; // buffer for UART
    char buf[20];
    //char buf2[200];
    char speed_buf[60];

    // --------------------------------------------

    int pwm_period = 50; // set PWM period

    initHallABC(); // initialise ISRs
    success = initMotorLib(pwm_period, &eb); // initialise MotorLib

    if(success)
    {
        // test success
        System_printf("MotorLib successfully initialised \n");
        System_flush();
    }

    enableMotor();
    setDuty(6);
    readABC();
    updateMotor(hA, hB, hC);

    UInt state;

    motorRunning = true;

    while(1)
    {
        //state = Event_pend(evtHandle, Event_Id_None, Event_Id_01, BIOS_WAIT_FOREVER);

        //switch(state):
        //                case(Event_Id_01):
        //                    motor
        //if (motorRunning == true)
        //{
             // first motor drive
        //}

        if (print_from_clock == true)
        {
            //sprintf(speed_buf, "Test: %d\n\r", dummy);
            //UART_write(uart, speed_buf, sizeof(speed_buf));


            //int32_t rpm_print = (int32_t)rpm_avg;
            //UART_write(uart, &rpm_avg, sizeof(rpm_avg));
            //int32_t rpm_print_act = (int32_t)rpm_actual;
            //UART_write(uart, &rpm_actual, sizeof(rpm_actual));

            //UART_write(uart, &acceleration, sizeof(acceleration));
            //UART_write(uart, &acceleration_avg, sizeof(acceleration_avg));

            print_from_clock = false;
        }

    }
}



/**
 * !! Clock SWI THREAD !!
 * Calculates the speed of the motor in RPM
 *      - gets the change in position, deltaPos, from the
 *        Hardware interrupt. Calculates the rotations (sector
 *        of a full rotation) and divides this by clock_time
 *        (in minutes) for revolutions per minute. *
 */
Void clk0Fxn(UArg arg0)
{
    clock_count++;

    rpm_sum = rpm_actual + rpm_sum;
    acceleration_sum = acceleration + acceleration_sum;

    if (clock_count == SAMPLE_SIZE)
    {
        rpm_avg = (float) rpm_sum/SAMPLE_SIZE;
        acceleration_avg = acceleration_sum/ACCEL_SIZE;
        //rpm_avg_c = (float) rpm_sum_c/SAMPLE_SIZE;

        clock_count = 0;
        rpm_sum = 0;
        acceleration_sum = 0;
    }


    /* Calculate Acceleration */
    delta_rpm = rpm_avg - rpm_prev;
    acceleration = delta_rpm/clock_time_secs;
    rpm_prev = rpm_avg;

    print_from_clock = true;
}

Void clk1Fxn(UArg arg0)
{
    error = rpm_int - rpm_avg;
    integral_error = integral_error + error;
    output = Kp*error + Ki*integral_error;
    setDuty((uint16_t)output);
}

void speedFxn(UArg arg0)
{
    rpm_int = rpm_avg;
    rpm_desired = rpm_desired + 500;
    if(rpm_desired > 4000)
    {
        rpm_desired = 300;
    }
}

void accelFxn(UArg arg0)
{

    if (rpm_int < rpm_desired)
    {
        // every 10ms, increment by 5rpm ( == 5/0.01 = 500RPM/s)
        rpm_int = rpm_int + 5;
    }
    else if (rpm_int > rpm_desired)
    {
        // every 10ms, decrement by 5rpm
        rpm_int = rpm_int - 5;
    }

}

/**
 * ADC
 */

#define ADC_SEQ 1 // sequencer 1 -> 4 samples
#define ADC_STEP 0

void ADC0_Init() //ADC0 on PE3
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

     /*
     *  GPIOPinTypeADC configures Port E Pin 3 for use as an
     *  ADC converter input.
     *
     *  Lecture - Makes GPIO an input and sets them to analog
     */
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

    /*
     *  Configures the trigger source and priority of a sample
     *  sequence.
     *  1. base: base address of the ADC module
     *  2. sequenceNum: the sample sequence number
     *  3. trigger source that initiates sample sequence
     *  4. priority - relative priority of the sample sequence
     *                with other sample sequences
     */
    ADCSequenceConfigure(ADC0_BASE, ADC_SEQ, ADC_TRIGGER_PROCESSOR, 0);


    /*
     * Configure a step of the sample sequencer
     * 1. Base address of the ADC module
     * 2. Sample sequence number
     * 3. Step to be configured
     * 4. Configuration of this step:
     *      - Select channel 0 (AIN0)
     *      - Defined as the last in the sequence (ADC_CTL_END)
     *      - cause an interrupt when complete (ADC_CTL_IE)
     */
    ADCSequenceStepConfigure(ADC0_BASE, ADC_SEQ, ADC_STEP, ADC_CTL_IE |
                             ADC_CTL_CH0 | ADC_CTL_END);

    /*
     * Enable a sample sequence. Allow specified sample sequence
     * to be captured when trigger is detected.
     */
    ADCSequenceEnable(ADC0_BASE, ADC_SEQ);

    /*
     * Clears sample sequence interrupt source.
     */
    ADCIntClear(ADC0_BASE, ADC_SEQ);
}

int getSpeed( void )
{
    return (int) rpm_avg;
}

void setSpeed( int rpm_ui )
{
    rpm_desired = rpm_ui;
}

void startMotor( bool UI_motorRun )
{
    motorRunning = UI_motorRun;
}


void initMotor( void )
{

    ADC0_Init();

    Error_init(&eb);

       Board_initGeneral(); // calls SysCtlPeripheralEnable for all GPIO ports
       Board_initGPIO();    // calls the GPIO_init function -> initialises

       Board_initUART(); // initialise UART

       /* Setup TASK thread */
       Task_Params taskParams;
       Task_Params_init(&taskParams);
       taskParams.stackSize = TASKSTACKSIZE;
       taskParams.arg0 = 1000;
       taskParams.stack = &task0Stack;
       taskParams.instance->name = "task";
       taskParams.priority = 1;
       Task_construct(&task0Struct, (Task_FuncPtr)motorFxn, &taskParams, NULL);

       Types_FreqHz cpuFreq;
       BIOS_getCpuFreq(&cpuFreq);

       /* Setup Clock SWI */
       Clock_Params clkParams;
       Clock_Params_init(&clkParams);
       clkParams.period = 10;
       clkParams.startFlag = TRUE;

       /* Construct a periodic Clock Instance with period = 5 system time units */
       Clock_create((Clock_FuncPtr)clk0Fxn, 10, &clkParams, NULL);

       clkParams.period = 10;
       clkParams.startFlag = TRUE;
       Clock_create((Clock_FuncPtr)clk1Fxn, 10, &clkParams, NULL);

       clkParams.period = 5000;
       clkParams.startFlag = TRUE;
       Clock_create((Clock_FuncPtr)speedFxn, 5000, &clkParams, NULL);

       clkParams.period = 10;
       clkParams.startFlag = TRUE;
       Clock_create((Clock_FuncPtr)accelFxn, 10, &clkParams, NULL);

       //clkParams.period =

       /* Setup mailbox buffer for filtering */

       /* Setup message box for filtering */
       //Mailbox_Params mbxParams;
       MsgObj *msg = msg_mem;

       int i;
       queue = Queue_create(NULL, NULL);

       // increment the pointer (memory region)
       // increment the counter
       for(i = 0; i < NUMSGS; msg++, i++) {
           Queue_put(queue, &(msg->elem));
       }
       /* End Msg code */

       /* Setup Semaphore */
       Semaphore_Params semParams;
       Semaphore_Params_init(&semParams);
       semParams.mode = Semaphore_Mode_BINARY;
       Semaphore_construct(&semStruct, 0, &semParams);

       semHandle = Semaphore_handle(&semStruct);
       /* End Sem code */

       Event_construct(&evtStruct, NULL);
       evtHandle = Event_handle(&evtStruct);

       if (evtHandle == NULL)
       {
           System_abort("Event creation failed");
       }

       System_printf("Run------------------------------\n");

       /* Create GateHwi for critical section */
       GateHwi_Params_init(&gHwiprms);
       gateHwi = GateHwi_create(&gHwiprms, NULL);

       if (gateHwi == NULL)
       {
           System_abort("Gate failed");
       }
}
