/*
 * lis3dh.h
 *
 *  Created on: May 23, 2017
 *      Author: beam
 */

#ifndef COMPONENTS_BUS_I2C_INCLUDE_LIS3DH_H_
#define COMPONENTS_BUS_I2C_INCLUDE_LIS3DH_H_

//Device Registers
#define LIS3DH_STATUS_REG_AUX         0x07
#define LIS3DH_OUT_ADC1_L             0x08
#define LIS3DH_OUT_ADC1_H             0x09
#define LIS3DH_OUT_ADC2_L             0x0A
#define LIS3DH_OUT_ADC2_H             0x0B
#define LIS3DH_OUT_ADC3_L             0x0C
#define LIS3DH_OUT_ADC3_H             0x0D
#define LIS3DH_INT_COUNTER_REG        0x0E
#define LIS3DH_WHO_AM_I               0x0F

#define LIS3DH_TEMP_CFG_REG           0x1F
#define LIS3DH_CTRL_REG1              0x20
#define LIS3DH_CTRL_REG2              0x21
#define LIS3DH_CTRL_REG3              0x22
#define LIS3DH_CTRL_REG4              0x23
#define LIS3DH_CTRL_REG5              0x24
#define LIS3DH_CTRL_REG6              0x25
#define LIS3DH_REFERENCE              0x26
#define LIS3DH_STATUS_REG2            0x27
#define LIS3DH_OUT_X_L                0x28
#define LIS3DH_OUT_X_H                0x29
#define LIS3DH_OUT_Y_L                0x2A
#define LIS3DH_OUT_Y_H                0x2B
#define LIS3DH_OUT_Z_L                0x2C
#define LIS3DH_OUT_Z_H                0x2D
#define LIS3DH_FIFO_CTRL_REG          0x2E
#define LIS3DH_FIFO_SRC_REG           0x2F
#define LIS3DH_INT1_CFG               0x30
#define LIS3DH_INT1_SRC               0x31
#define LIS3DH_INT1_THS               0x32
#define LIS3DH_INT1_DURATION          0x33

#define LIS3DH_CLICK_CFG              0x38
#define LIS3DH_CLICK_SRC              0x39
#define LIS3DH_CLICK_THS              0x3A
#define LIS3DH_TIME_LIMIT             0x3B
#define LIS3DH_TIME_LATENCY           0x3C
#define LIS3DH_TIME_WINDOW            0x3D

//This struct holds the settings the driver uses to do calculations
typedef struct /* SensorSettings */
{
	//ADC and Temperature settings
	uint8_t adcEnabled;
	uint8_t tempEnabled;

	//Accelerometer settings
	uint16_t accelSampleRate;  //Hz.  Can be: 0,1,10,25,50,100,200,400,1600,5000 Hz
	uint8_t accelRange;      //Max G force readable.  Can be: 2, 4, 8, 16

	uint8_t xAccelEnabled;
	uint8_t yAccelEnabled;
	uint8_t zAccelEnabled;
	
	//Fifo settings
	uint8_t fifoEnabled;
	uint8_t fifoMode; //can be 0x0,0x1,0x2,0x3
	uint8_t fifoThreshold;
} lis3dh_settings, *pLis3dh_settings;


#endif /* COMPONENTS_BUS_I2C_INCLUDE_LIS3DH_H_ */
