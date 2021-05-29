#include "bmi160.h"
/*
 * NOTES:
 * 2.1      data from accelerator sensor is always sampled at 1600Hz
 * 2.1      raw data is digitally filtered (using a LPF) to output data rate configured in the register ACC_CONF (0x40)
 * 2.4.1    Accelerometer data can only be processed in normal power mode or low powered mode
 * 2.4.1    when acc_us is set the normal mode, acc_bmp (filter bandwidth) needs to be set to 0b010 (3db cutoff = 20.25Hz)
 * 2.5      FIFO is only supported for low power applications (not necessary for this task)
 *
 * 2.6 Interrupt Controllers (13 signals)
 *          Potentially need the: any-motion (slope) detection,
 *          significant motion, step detector, low-g/high-g (want high-g), and data ready
 * 2.6.1    Any-Time motion detection (accel) generates an interrupt when the abs value of acceleration
 *          exceeds a given threshold int_anym_th for a consecutive number of data points int_anym_dur
 *          > The register is 16 bits and resets if data points stays below int_anym_th for int_anym_dur (2^16-1 = 65535)
 */

/////////////////////////////////////
// BMI REGISTER MAP BASE ADDRESSES //
/////////////////////////////////////
#define CHIPID              0x00
#define ERR_REG             0x02
#define PMU_STATUS          0x03
#define DATA                0x04
#define SENSORTIME          0x18
#define STATUS              0x1B
#define INT_STATUS          0x1C //
#define TEMPERATURE         0x20
#define FIFO_LENGTH         0x22
#define FIFO_DATA           0x24
#define ACC_CONF            0x40 //
#define ACC_RANGE           0x41 //
#define GYR_CONF            0x42
#define GYR_RANGE           0x43
#define MAG_CONF            0x44
#define FIFO_DOWNS          0x45
#define FIFO_CONFIG         0x46
#define MAG_IF              0x4B
#define INT_EN              0x50 // -> NOTE: INT_EN+1 0x51
#define INT_OUT_CTRL        0x53 //
#define INT_LATCH           0x54 //
#define INT_MAP             0x55 //
#define INT_DATA            0x58
#define INT_LOWHIGH         0x5A // -> NOTE: INT_LOWHIGH+3 0x5D & INT_LOWHIGH+4 0x5E
#define INT_HIGH_DUR        0x5D //
#define INT_HIGH_TH         0x5E //
#define INT_MOTION          0x5F
#define INT_TAP             0x63
#define INT_ORIENT          0x65
#define INT_FLAT            0x67
#define FOC_CONF            0x69 // register digitally (low pass) filtered data stored in
#define CONF                0x6A //
#define INF_CONF            0x6B
#define PMU_TRIGGER         0x6C
#define SELF_TEST           0x6D
#define NV_CONF             0x70
#define OFFSET              0x71
#define STEP_CNT            0x78
#define STEP_CONF           0x7A
#define CMD                 0x7E //

volatile const int SENSITIVITY = 8192; // LSB/g (typical value temp of 25 degrees)

bool BMI160Init(I2C_Handle i2c) {
    uint8_t normalMode = 0x11;
    writeI2C(i2c, FOC_CONF, CMD, &normalMode);
    Task_sleep(5);

    // 100/(2^(val(acc_odr)) = 100/(2^(8-0b1001)) = 100/(2^(8-9)) = 200Hz (output data rate)
    uint8_t outputDataRate = 0x29;
    writeI2C(i2c, FOC_CONF, CMD, &outputDataRate);

    // Using +/-4g acceleration range
    uint8_t range = 0x05;
    writeI2C(i2c, FOC_CONF, ACC_RANGE, &range);

    // enable the interrupt for the high acceleration threshold
    uint8_t highGInterruptEnable = 0x07;
    writeI2C(i2c, FOC_CONF, INT_EN+1, &highGInterruptEnable); // INT_EN + 1 = 0x50 + 1 = 0x51

    // INT_OUT_CTRL (0x53)
    uint8_t output = 0x0F; // 0b1111 // Not sure on last bit
    writeI2C(i2c, FOC_CONF, INT_OUT_CTRL, &output);

    // INT_LATCH (0x54)
    uint8_t latch = 0x1A; // 0b11010
    writeI2C(i2c, FOC_CONF, INT_LATCH, &latch);

    // INT_MAP (0x55-0x57)
    uint8_t mapping = 0x02; // 0b10
    writeI2C(i2c, FOC_CONF, INT_MAP, &mapping);


    // set the threshold duration that triggers the interrupt to 2.5ms
    // value(int_high_th<7:0> + 1) * 2.5ms
    uint8_t int_high_dur = 0x00;
    writeI2C(i2c, FOC_CONF, INT_HIGH_DUR, &int_high_dur);

    // value(int_high_th<7:0>) * 15.63mg (4g range)
    // initialise to max threshold: 0xFF * 0.01563 = 2^8-1 * 0.01563 = 3.98565 g
    // High-g threshold (15.63mG per bit)
    uint8_t int_high_threshold = 0xFF;
    writeI2C(i2c, FOC_CONF, INT_HIGH_TH, &int_high_threshold);
    return true;
}

/*
 * Take user input threshold and convert to binary to place in threshold register convert
 * from m/s^2 to g by dividing by 9.8 and multiply 9.8 by 0.01563 as this is the
 * g force the sensor samples in when set to +/-4g range in the ACC_RANGE register
 */
bool accelThreshold(I2C_Handle i2c, uint8_t threshold) {

    // converting m/s^2 to g where sensor is set to +/-4g range
    // value(int_high_th<7:0>) = threshold_mps2 / (g-force_per_bit * gravity_accel )
    // NOTE: g-force per bit when set to +/-4g range is 15.63mg
    uint8_t int_high_th = (float) threshold / (0.01563 * 9.81); // name of threshold register
    // write converted register value to acceleration threshold register
    writeI2C(i2c, FOC_CONF, INT_HIGH_TH, &int_high_th);

    return true;
}

/*
 * Acceleration data is stored in DATA registers:
 * 0x12 ACC_X<7:0>  (LSB)
 * 0x13 ACC_X<15:8> (MSB)
 * 0x14 ACC_Y<7:0>  (LSB)
 * 0x15 ACC_Y<15:8> (MSB)
 * 0x16 ACC_Z<7:0>  (LSB)
 * 0x17 ACC_Z<15:8> (MSB)
 */
bool getAccelVector(I2C_Handle i2c, accel_vector *av) {
    uint8_t rxBuffer[6] = {0}; // stores a uint8_t for 3 axis'
    // rxBuffer used to store {X MSB, X LSB, Y MSB, Y LSB, Z MSB, Z LSB}

    readI2C(i2c, FOC_CONF, DATA+14, rxBuffer); // DATA+14 = 0x04 + 14 = 0x12

    // Combine separate 8 bit data into 16 bit (stored in lsb msb order)

    // Combining 2 uint8_t to a uint16_t then cast to int16_t as we only want absolute magnitude
    // Take first two bytes (x values), swap the bytes to be in LSB+MSB order and store them as an uint
     int16_t xint = (uint16_t)(int16_t) (rxBuffer[1] << 8 | rxBuffer[0] >> 8 &0xFF);
     // Take second two bytes (y values), swap the bytes to be in LSB+MSB order and store them as an uint
     int16_t yint = (uint16_t)(int16_t) (rxBuffer[3] << 8 | rxBuffer[2] >> 8 &0xFF);
     // Take third two bytes (z values), swap the bytes to be in LSB+MSB order and store them as an uint
     int16_t zint = (uint16_t)(int16_t) (rxBuffer[5] << 8 | rxBuffer[4] >> 8 &0xFF);

    // Get Gs by dividing by resolution and m/s^2 by mulitplying by 9.8
    av->x = (float) (xint * 9.81) / SENSITIVITY;
    av->y = (float) (yint * 9.81) / SENSITIVITY;
    av->z = (float) (zint * 9.81) / SENSITIVITY;

    return true;
}



bool writeI2C(I2C_Handle i2c, uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *data) {
    uint8_t txBuffer[2];
    txBuffer[0] = ui8Reg;    // Register to be written to
    txBuffer[1] = data[0];  // Data to set the register with

    I2C_Transaction i2cTransaction;
    i2cTransaction.slaveAddress = ui8Addr;  // slave address to write to
    i2cTransaction.writeBuf = txBuffer;     // buffer containing the data
    i2cTransaction.writeCount = 2;          // write 2 bytes to the i2c
    i2cTransaction.readBuf = NULL;
    i2cTransaction.readCount = 0;

    if (!I2C_transfer(i2c, &i2cTransaction)) { // Send txBuffer over i2c to slave
        System_printf("I2C Bus fault\n");
    }
    return true;
}

bool readI2C(I2C_Handle i2c, uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *data) {
    uint8_t txBuffer[1] = {ui8Reg};

    I2C_Transaction i2cTransaction;
    i2cTransaction.slaveAddress = ui8Addr;
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = data;
    i2cTransaction.readCount = 6;   // Used for reading x, y, z MSB and LSB data

    if (!I2C_transfer(i2c, &i2cTransaction)) {
        System_printf("I2C Bus fault\n");
    }
    return true;

}
