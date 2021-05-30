/*
 * motorCode.c
 *
 *  Author: Raj Silari
 *  EGH456 Group 17
 */
/*
 *  ======== empty.c ========
 */

/* Standard Libraries */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* XDCtools Header files */
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
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/Hal/Timer.h>
#include <ti/sysbios/knl/Queue.h>

/* TivaWare Driver files */

#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART.h>

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


#define TOTAL_POSITIONS 12
#define SAMPLE_SIZE 100
#define ACCEL_SIZE 100
#define MAXOUTPUT 24
#define NUMSGS 50
#define ADC_SEQ 1 // sequencer 1 -> 4 samples
#define ADC_STEP 0

Semaphore_Struct semStruct;
Semaphore_Handle semHandle;


Clock_Struct clkStruct;
Clock_Handle clkHandle;

Hwi_Handle hallA, hallB, hallC;
Hwi_Handle adc0, adc1;


Error_Block eb;
Event_Struct evtStruct;
Event_Handle evtHandle;

UInt gateKey;
GateHwi_Handle gateHwi;
GateHwi_Params gHwiprms;

uint32_t g_ui32SysClock;

UART_Handle uart;

bool success, print_from_HWI, print_from_motor, print_from_clock;
volatile int motor = 0;
volatile int prevMotor = 0;
volatile int prevSysTick = 0;
volatile int delta_ticks, SysTicks;
volatile int deltaPos = 0;
volatile int current_motor;
volatile float clock_time_mins = 0.00016667;
volatile float clock_time_secs = 0.01;
//volatile float clock_time = 0.00166667;
//volatile float clock_time = 0.00416667;
//volatile float clock_time_mins = 0.000333333;
//volatile float clock_time_mins = 0.00016667/2;

volatile float rpm_actual = 0;
volatile float delta_time;
volatile float delta_rpm;
volatile float acceleration;
volatile float acceleration_avg;
volatile float acceleration_sum = 0;

volatile rpm_prev = 0;

volatile int error;
volatile float integral_error = 0;

volatile float rpm_desired = 200;
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
volatile float rpm_from_UI;

volatile float rpm_buf[5];

volatile bool motorState;
volatile bool running;


volatile bool e_event; // retrieved from sensor APIs
volatile bool e_stop; // scope limited to motorCode API

volatile int dummy = 0;

volatile int adc_test = 0;

/*!
 *  @brief  ISR to read the Hall Effect lines, calculate RPM and update the motor
 */
void  HallFxn()
{
    // Get current system ticks
    SysTicks = Clock_getTicks();

    /* Clear all interrupt sources */
    GPIO_clearInt(HALL_A);
    GPIO_clearInt(HALL_B);
    GPIO_clearInt(HALL_C);

    /* read in hall-effect values */
    hA = GPIO_read(HALL_A);
    hB = GPIO_read(HALL_B);
    hC = GPIO_read(HALL_C);

    // motor not required as the rpm is calculated in this HWI
    // so positional change is always 1
    motor++;

    // push motor again
    updateMotor(hA, hB, hC);

    /* feed values into updateMotor to drive the motor */

    /* delta time is change in ticks * time (mins) for one tick */
    delta_time = (min_per_sysTick*(SysTicks - prevSysTick));

    /* RPM calculation for ONE positional change */
    rpm_actual = rotation/delta_time;

    prevSysTick = SysTicks;

}

/*!
 *  @brief  ISR to read buffer from ADC0
 *
 *  @pre ADC0_Init() is called in setup
 *
 */
void ADCFxn()
{
    uint32_t pui32ADC0Value[1];

    //ADCProcessorTrigger(ADC0_BASE, ADC_SEQ);
    //ADC
    ADCIntClear(ADC0_BASE, ADC_SEQ);
    adc_test++;
}

/*!
 *  @brief  Setup all HWIs for each GPIO line, for Hall A/B/C, and enable interrupts
 *
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
    adc0 = Hwi_create(INT_ADC0SS0_TM4C123, (Hwi_FuncPtr)ADCFxn, &hwiParams, NULL);

    if (adc0 == NULL)
    {
        System_abort("ADC HWI creation failed...");
    }

    ADCIntEnable(ADC0_BASE, ADC_SEQ);

    //GPIO_enableInt(ADC0_BASE);
    //GPIOIntTypeSet(GPIO_PORTE_BASE, GPIO_PIN_3, GPIO_RISING_EDGE);
    //GPIODirModeSet(GPIO_PORTE_BASE, GPIO_PIN_3, GPIO_DIR_MODE_IN);

    System_printf("Done initHallABC\n");
    System_flush();
    //Board_BUTTON0  User switch 1
}


/*!
 *  @brief  Read the current state of the hall effect sensors
 */
void readABC()
{
    hA = GPIO_read(HALL_A);
    hB = GPIO_read(HALL_B);
    hC = GPIO_read(HALL_C);
}


/*!
 *  @brief  initialise the UART. Only used during development, not required
 *  during production.
 */
void initUART(void)
{
    /* UART used for printing purposes - not */
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
}

/*!
 *  @brief Clock SWI to check the states of the motor. Uses events
 *         to post the current state to the task thread.
 *
 *         Event_Id_00 ---> set the motor to IDLE
 *         Event_Id_01 ---> set the motor to STOP
 *         Event_Id_02 ---> set the motor to RUNNING (includes speed > 0 & at speed)
 *         Event_Id_03 ---> set e-stop condition
 *
 *  @params arg0
 */
void statesFxn(UARg, arg0)
{
    /* User has started the motor */
    if(motorState == true)
    {
        Event_post(evtHandle, Event_Id_00);
    }

    if(motorState == false)
    {
        Event_post(evtHandle, Event_Id_01);
    }

    if(running == true)
    {
        Event_post(evtHandle, Event_Id_02);
    }

    if(e_event == true)
    {
        Event_post(evtHandle, Event_Id_03);
    }

}


/*!
 *  @brief  Task thread for the motor. Responsible for calling the HWI setup and driving the
 *          initial updateMotor. A while loop is used to control the states of the system
 *          (speed up, speed down, e-stop). UART was used during testing and is not required
 *          for production.
 *
 *  @params arg0, arg1
 */
void motorFxn(UArg arg0, UArg arg1)
{
    initUART();

    if (uart == NULL)
    {
        // test UART
        System_abort("Error opening the UART");
    }
    // buffer for UART
    char buf[20];
    char speed_buf[60];

    /* DO NOT CHANGE - Kp and Ki are tuned to a pwm period of 50us */
    int pwm_period = 50; // set PWM period

    initHallABC();
    success = initMotorLib(pwm_period, &eb); // initialise MotorLib

    if(success)
    {
        // test success
        System_printf("MotorLib successfully initialised \n");
        System_flush();
    }

    enableMotor();
    setDuty(15);
    readABC();
    updateMotor(hA, hB, hC);

    UInt state;

    while(1)
    {

        /*
        state = Event_pend(evtHandle, Event_Id_NONE, (Event_Id_00 + Event_Id_01 + Event_Id_02 + Event_Id_03),
                           BIOS_WAIT_FOREVER);

        // IDLE - Start the motor
        if (state & Event_Id_00)
        {
            running = true;
            //enableMotor();
            rpm_desired = rpm_from_UI;
            readABC();
            updateMotor(hA, hB, hC);
        }

        // STOP - Speed = 0
        if (state & Event_Id_01)
        {
            running = false;
            rpm_desired = 0;

            if (rpm_avg < 100)
            {
                e_stop = false;
                e_event = false;
                error = 0;
                integral_error = 0;
                stopMotor(true);
            }
        }

        // STARTING AND RUNNING - Speed > 0
        if (state & Event_Id_02)
        {
            //updateMotor(hA, hB, hC);
            rpm_desired = rpm_from_UI;
        }

        // E-STOP
        if (state & Event_Id_03)
        {
            // trigger local estop for the deceleration
            e_stop = true;

            // set the motorState to false. In the statesFxn SWI, this will trigger a posting
            // of Event_Id_01, which causes the motor to stop. With the above e-stop as true,
            // the accelLimitFxn will decrement by 10RPM every 10ms.
            motorState = false;
        }
        */


        if (print_from_clock == true)
        {
            //System_printf("%d\n", adc_test);
            sprintf(speed_buf, "Test: %d\n\r", adc_test);
            UART_write(uart, speed_buf, sizeof(speed_buf));
            //int32_t rpm_print = (int32_t)rpm_avg;
            //UART_write(uart, &rpm_avg, sizeof(rpm_avg));
            //int32_t rpm_print_act = (int32_t)rpm_actual;
            //UART_write(uart, &adc_test, sizeof(adc_test));
            //print_from_clock = false;
        }

    }
}

/*!
 *  @brief  Filter - Calculates the average RPM from the actual RPM calculated
 *          by the HWI. Uses a sample of SAMPLE_SIZE at a frequency
 *          of 100Hz (clock period 10ms).
 *
 *  @params arg0
 */
Void filterFxn(UArg arg0)
{
    clock_count++;

    rpm_sum = rpm_actual + rpm_sum;
    acceleration_sum = acceleration + acceleration_sum;

    if (clock_count == SAMPLE_SIZE)
    {
        rpm_avg = (float) rpm_sum/SAMPLE_SIZE;
        acceleration_avg = acceleration_sum/ACCEL_SIZE;

        clock_count = 0;
        rpm_sum = 0;
        acceleration_sum = 0;
    }

    /* Calculate Acceleration -- for testing, not required for production */
    delta_rpm = rpm_avg - rpm_prev;
    acceleration = delta_rpm/clock_time_secs;
    rpm_prev = rpm_avg;

    print_from_clock = true;
}

/*!
 *  @brief  Proportional Integral Control for the speed, with a frequency of 100Hz.
 *
 *  @params arg0
 */
Void PIControlFxn(UArg arg0)
{
    error = rpm_int - rpm_avg;
    integral_error = integral_error + error;
    output = Kp*error + Ki*integral_error;
    setDuty((uint16_t)output);
}

/*!
 *  @brief Simulates UI speed increase. Used for testing.
 *
 *  @params arg0
 */
/*
void speedFxn(UArg arg0)
{
    rpm_int = rpm_avg;
    rpm_desired = rpm_desired + 500;
    if(rpm_desired > 4000)
    {
        rpm_desired = 300;
    }
}
*/

/*!
 *  @brief Limits the acceleration of the motor to 500RPM/s. This Clock runs has a
 *         frequency of 100Hz, and uses an intermediate RPM that increments at
 *         5RPM every 0.01s - results in an acceleration of 500RPM/s.
 *
 *         The deceleration has a conditional to check if an e-stop is triggered
 *         by the task thread.
 *
 *  @params arg0
 */
void accelLimitFxn(UArg arg0)
{
    if (rpm_int < rpm_desired)
    {
        // every 10ms, increment by 5rpm ( == 5/0.01 = 500RPM/s)
        rpm_int = rpm_int + 5;
    }
    else if (rpm_int > rpm_desired)
    {
        // check if an e-stop condition has occured.
        if (e_stop == true)
        {
            // if true, every 10ms, decrement by 10RPM (10RPM/0.01s = 1000RPM/s).
            rpm_int = rpm_int - 100;
        }
        else
        {
            // if false, every 10ms, decrement by 5RPM as normal
            rpm_int = rpm_int - 5;
        }
    }
}


void waitFxn(UArg arg0)
{
    Semaphore_post(semHandle);
}


/**
 * ADC
 */


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
                             ADC_CTL_CH4 | ADC_CTL_END);

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

/*!
 *  @brief Gets the current speed of the motor
 */
int getSpeed( void )
{
    return (int) rpm_avg;
}

bool getState(void)
{
    return running;
}

void set_estop(void)
{
    e_stop = true;
}
/*
 * @brief Once the motor has started, if user changes the speed then
 *
 */

/*!
 *  @brief Sets the desired RPM from the UI.
 *
 *  @params rpm_ui  the UI selected RPM
 */
void setSpeed(uint32_t rpm_ui)
{
    rpm_from_UI = (float) rpm_ui;
}

/*!
 *  @brief sets the motor start condition
 *
 *  @params UI      the boolean condition to start the motor
 */
void startMotor( bool UI_motorRun )
{
    motorState = UI_motorRun;
    dummy++;

    if(dummy == 4)
    {
        e_event = true; // hit stop motor a second time
    }
}

/*!
 *  @brief  Initialise the motorCode file.
 *
 *  @params arg0
 */
void initMotor( void )
{

    ADC0_Init();

    Error_init(&eb);

    Board_initGeneral(); // calls SysCtlPeripheralEnable for all GPIO ports
    Board_initGPIO();    // calls the GPIO_init function -> initialises

    Board_initUART(); // initialise UART

    Types_FreqHz cpuFreq;
    BIOS_getCpuFreq(&cpuFreq);

    /* Setup Clock SWI */
    Clock_Params clkParams;
    Clock_Params_init(&clkParams);
    clkParams.period = 10;
    clkParams.startFlag = TRUE;

    /* Construct a periodic Clock Instance with period = 5 system time units */
    Clock_create((Clock_FuncPtr)filterFxn, 10, &clkParams, NULL);

    clkParams.period = 10;
    clkParams.startFlag = TRUE;
    Clock_create((Clock_FuncPtr)PIControlFxn, 10, &clkParams, NULL);

    //clkParams.period = 5000;
    //clkParams.startFlag = TRUE;
    //Clock_create((Clock_FuncPtr)speedFxn, 5000, &clkParams, NULL);

    clkParams.period = 10;
    clkParams.startFlag = TRUE;
    Clock_create((Clock_FuncPtr)accelLimitFxn, 10, &clkParams, NULL);

    clkParams.period = 10;
    clkParams.startFlag = TRUE;
    Clock_create((Clock_FuncPtr)waitFxn, 10, &clkParams, NULL);

    clkParams.period = 10;
    clkParams.startFlag = TRUE;
    Clock_create((Clock_FuncPtr)statesFxn, 10, &clkParams, NULL);

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
