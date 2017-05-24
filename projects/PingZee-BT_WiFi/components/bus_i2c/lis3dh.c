/*
 * lis3dh.c
 *
 *  Created on: May 23, 2017
 *      Author: beam
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "app.h"
#include "board.h"
#include "lis3dh.h"

static const char *TAG = "lis3dh";

#define lis3dhQUEUE_LENGTH			( 2 )
#define LIS3DH_NOT_PRESENT		(0x00)
#define LIS3DH_PRESENT				(0x01)
#define LIS3DH_INIT					(0x02)
#define LIS3DH_OFF					(0x80)
#define ESP_INTR_FLAG_DEFAULT		0


static xQueueHandle xLis3dhInQueue = NULL;
static xQueueHandle xLis3dhOutQueue = NULL;
static xSemaphoreHandle xLis3dhMutex = NULL;

static pLis3dhMessage master_pLis3dhMsg = NULL;   // current transaction ..
static uint8_t ucLis3dhStatus = LIS3DH_NOT_PRESENT;
static lis3dh_settings lis3dh_default_settings;
static pLis3dh_settings current_settings = NULL;
static uint8_t polling_state = 0x00;

static void prvLis3dhTask( void *pvParameters );
static esp_err_t  Lis3dh_readRegister(uint8_t* outputPointer, uint8_t offset);
static esp_err_t  Lis3dh_writeRegister(uint8_t offset, uint8_t dataToWrite);
static esp_err_t  Lis3dh_readRegisterRegion(uint8_t *outputPointer , uint8_t offset, uint8_t length);
static esp_err_t  Lis3dh_readRegisterInt16( int16_t* outputPointer, uint8_t offset );
static void Lis3dh_setup_default(void);
static void Lis3dh_applySettings( pLis3dh_settings settings);
static void Lis3dh_config_Intterupts(void);
static void Lis3dh_gpio_cfg(void);
static float Lis3dh_calcAccel( int16_t input );
static int16_t Lis3dh_readRawAccelX( void );
static int16_t Lis3dh_readRawAccelY( void );
static int16_t Lis3dh_readRawAccelZ( void );
static float Lis3dh_readFloatAccelX( void );
static float Lis3dh_readFloatAccelY( void );
static float Lis3dh_readFloatAccelZ( void );
static void IRAM_ATTR lis3dh_gpio_isr_handler(void* arg);



/*-----------------------------------------------------------*/

void vLis3dhStart( uint16_t usStackSize, portBASE_TYPE uxPriority )
{
	/* Create the queues used by tasks and interrupts to send/receive SPI data. */
	xLis3dhInQueue = xQueueCreate( lis3dhQUEUE_LENGTH, sizeof(Lis3dhMessage_t) );
	xLis3dhOutQueue = xQueueCreate( 1, sizeof(Lis3dhMessage_t) );

	/* If the queue could not be created then don't create any tasks that might
	attempt to use the queue. */
	if( xLis3dhInQueue != NULL && xLis3dhOutQueue != NULL)
	{
	
		/* Create the semaphore used to access the SPI transmission. */
		xLis3dhMutex = xSemaphoreCreateMutex();
		configASSERT( xLis3dhMutex );

		/* Create that task that handles the I2C Master itself. */
		xTaskCreate(prvLis3dhTask,	/* The task that implements the command console. */
					(const char *const) "LIS3DH",	/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
					usStackSize,					/* The size of the stack allocated to the task. */
					NULL,						/* The parameter is not used, so NULL is passed. */
					uxPriority,					/* The priority allocated to the task. */
					NULL );						/* A handle is not required, so just pass NULL. */

	}
}

/*-----------------------------------------------------------*/
static void prvLis3dhTask( void *pvParameters )
{
	Lis3dhMessage_t _Lis3dhMsg;   // current transaction ..
	( void ) pvParameters;
	pLis3dh_settings settings;

		master_pLis3dhMsg = &_Lis3dhMsg;

		while(!isI2C0Ready()) {
			vTaskDelay(100);
		}

		Lis3dh_gpio_cfg();
		Lis3dh_setup_default();
		current_settings = &lis3dh_default_settings;
		
		while(1)
		{	
			if( xQueueReceive( xLis3dhInQueue, master_pLis3dhMsg, portMAX_DELAY) == pdPASS)
			{
				if (master_pLis3dhMsg ->app_msg.cmd) {
					switch (master_pLis3dhMsg ->app_msg.cmd)
					{
					case APPMSG_GET_STACK_ROOM:
							master_pLis3dhMsg->app_msg.d.v= uxTaskGetStackHighWaterMark(NULL);
							if (master_pLis3dhMsg->app_msg.app__doneCallback != NULL)
								master_pLis3dhMsg->app_msg.app__doneCallback(master_pLis3dhMsg);
						break;
					case APPMSG_LIS3DH_CHECK_PRESENT: 
						if (isI2C0Ready()) {		
						//Check the ID register to determine if the operation was a success.
							uint8_t readCheck;
							master_pLis3dhMsg->app_msg.d.param[0] = LIS3DH_NOT_PRESENT;
							Lis3dh_readRegister(&readCheck, LIS3DH_WHO_AM_I);
							if( readCheck == 0x33 )
							{
								ucLis3dhStatus = LIS3DH_PRESENT;
								master_pLis3dhMsg->app_msg.d.param[0] = LIS3DH_PRESENT;
								
							}
						}else{
							// [ADK] Temporary, until we will have ALL tasks implemented
							VApplicationGeneralFault;
						}
						
						break;
					case APPMSG_LIS3DH_SETUP:
						if (ucLis3dhStatus == LIS3DH_PRESENT ) {
							settings = (pLis3dh_settings)master_pLis3dhMsg->app_msg.d.ptr;
							if (settings == NULL) 
								Lis3dh_applySettings(&lis3dh_default_settings);
							else	{							
								Lis3dh_applySettings(settings);
								current_settings = settings;
							}
							Lis3dh_config_Intterupts();
						}
						break;	
					}
				}else{
					if (polling_state == 0x00) {
						master_pLis3dhMsg->accl = Lis3dh_readFloatAccelX();
						polling_state = 0x01;
					}else
					if (polling_state == 0x01) {
						master_pLis3dhMsg->accl = Lis3dh_readFloatAccelY();
						polling_state = 0x02;
					}else
					if (polling_state == 0x02) {
						master_pLis3dhMsg->accl = Lis3dh_readFloatAccelZ();
						polling_state = 0x00;
					}
				}
			}
			if (master_pLis3dhMsg->lis3dh__doneCallback)
				master_pLis3dhMsg->lis3dh__doneCallback((void *)master_pLis3dhMsg);
			
		}
}

/*-----------------------------------------------------------*/

portBASE_TYPE Lis3dhMsgPut(pLis3dhMessage msg)
{
	if(xQueueSend(xLis3dhInQueue, msg, ( portTickType ) 10) != pdPASS)
		{
			VApplicationGeneralFault;
		}
	return pdPASS;
}

portBASE_TYPE isLis3dhPresents(void) 
{
	if (ucLis3dhStatus == LIS3DH_PRESENT)  return pdTRUE;
	return pdFALSE;
}

/*-----------------------------------------------------------*/
portBASE_TYPE  Lis3dhOnDone(void *pvParameters)
{
	pLis3dhMessage msg = (pLis3dhMessage)pvParameters;

	if(xQueueSend(xLis3dhOutQueue, msg, ( portTickType ) (10 /portTICK_RATE_MS)) != pdPASS)
	{
		VApplicationGeneralFault;
	}
		
	return pdPASS;
}	
portBASE_TYPE Lis3dhMsgGet(pLis3dhMessage msg)
{
	return xQueueReceive(xLis3dhOutQueue, msg, portMAX_DELAY);
}

/*-----------------------------------------------------------*/


//****************************************************************************//
//
//  ReadRegister
//
//  Parameters:
//    *outputPointer -- Pass &variable (address of) to save read data to
//    offset -- register to read
//
//****************************************************************************//
static esp_err_t  Lis3dh_readRegister(uint8_t* outputPointer, uint8_t offset) 
{
	I2CMessage_t msg;
	uint8_t  addr[2], value;

	addr[0] = offset;
		
	msg.tx.length = 1;
	msg.tx.chip = LIS3DH_I2C_ADDR;
	msg.tx.buffer = &addr[0];
	msg.rx.length = 1;
	msg.rx.chip = LIS3DH_I2C_ADDR;
	msg.rx.buffer = &value;
	msg.i2c__doneCallback = I2C0OnDone;
	I2C0Take(NULL);
	I2C0MsgPut(&msg);
	while (I2C0MsgGet(&msg) != pdFAIL) {
		break;
	}
	I2C0Give(NULL);

//	printf("value 0x%02x\r\n", value);
	*outputPointer = value;
	return ESP_OK;	

}

//****************************************************************************//
//
//  writeRegister
//
//  Parameters:
//    offset -- register to write
//    dataToWrite -- 8 bit data to write to register
//
//****************************************************************************//
static esp_err_t  Lis3dh_writeRegister(uint8_t offset, uint8_t dataToWrite)
{
	I2CMessage_t msg;
	uint8_t  addr[2];

	addr[0] = offset;
	addr[1] = dataToWrite;

	msg.tx.length = 2;
	msg.tx.chip = LIS3DH_I2C_ADDR;
	msg.tx.buffer = &addr;
	msg.rx.length = 0;
	msg.rx.chip = LIS3DH_I2C_ADDR;
	msg.rx.buffer = NULL;
	msg.i2c__doneCallback = I2C0OnDone;
	I2C0Take(NULL);
	I2C0MsgPut(&msg);
	while (I2C0MsgGet(&msg) != pdFAIL) {
		break;
	}
	I2C0Give(NULL);
	return ESP_OK;	
}

//****************************************************************************//
//
//  ReadRegisterRegion
//
//  Parameters:
//    *outputPointer -- Pass &variable (base address of) to save read data to
//    offset -- register to read
//    length -- number of bytes to read
//
//  Note:  Does not know if the target memory space is an array or not, or
//    if there is the array is big enough.  if the variable passed is only
//    two bytes long and 3 bytes are requested, this will over-write some
//    other memory!
//
//****************************************************************************//
static esp_err_t  Lis3dh_readRegisterRegion(uint8_t *outputPointer , uint8_t offset, uint8_t length)
{
	I2CMessage_t msg;
	uint8_t  addr[2];

	addr[0] = offset | 0x80;  //turn auto-increment bit on, bit 7 for I2C
	msg.tx.length = 1;
	msg.tx.chip = LIS3DH_I2C_ADDR;
	msg.tx.buffer = &addr[0];
	msg.rx.length = length;
	msg.rx.chip = LIS3DH_I2C_ADDR;
	msg.rx.buffer = outputPointer;
	msg.i2c__doneCallback = I2C0OnDone;
	I2C0Take(NULL);
	I2C0MsgPut(&msg);
	while (I2C0MsgGet(&msg) != pdFAIL) {
		break;
	}
	I2C0Give(NULL);
	return ESP_OK;	

}

//****************************************************************************//
//
//  readRegisterInt16
//
//  Parameters:
//    *outputPointer -- Pass &variable (base address of) to save read data to
//    offset -- register to read
//
//****************************************************************************//
static esp_err_t  Lis3dh_readRegisterInt16( int16_t* outputPointer, uint8_t offset )
{
	uint8_t myBuffer[2];
	esp_err_t  returnError = Lis3dh_readRegisterRegion(myBuffer, offset, 2);  //Does memory transfer
	int16_t output = (int16_t)myBuffer[0] | (int16_t)(myBuffer[1] << 8);
	*outputPointer = output;
	return returnError;
}

/*-----------------------------------------------------------*/


//****************************************************************************//
//
//  Configuration section
//
//
//****************************************************************************//

static void Lis3dh_setup_default(void)
{
	//Construct with these default settings
	//ADC stuff
	lis3dh_default_settings.adcEnabled = 0;
	
	//Temperature settings
	lis3dh_default_settings.tempEnabled = 0;

	//Accelerometer settings
	lis3dh_default_settings.accelSampleRate = 50;  //Hz.  Can be: 0,1,10,25,50,100,200,400,1600,5000 Hz
	lis3dh_default_settings.accelRange = 2;      //Max G force readable.  Can be: 2, 4, 8, 16

	lis3dh_default_settings.xAccelEnabled = 1;
	lis3dh_default_settings.yAccelEnabled = 1;
	lis3dh_default_settings.zAccelEnabled = 1;

	//FIFO control settings
	lis3dh_default_settings.fifoEnabled = 0;
	lis3dh_default_settings.fifoThreshold = 20;  //Can be 0 to 32
	lis3dh_default_settings.fifoMode = 0;  //FIFO mode.

}

static void Lis3dh_applySettings( pLis3dh_settings settings)
{
	uint8_t dataToWrite = 0;  //Temporary variable

	//Build TEMP_CFG_REG
	dataToWrite = 0; //Start Fresh!
	dataToWrite = ((settings->tempEnabled & 0x01) << 6) | ((settings->adcEnabled & 0x01) << 7);
	//Now, write the patched together data
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_TEMP_CFG_REG(0x%02x) := 0x%02x\r\n", LIS3DH_TEMP_CFG_REG, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_TEMP_CFG_REG, dataToWrite);
	
	//Build CTRL_REG1
	dataToWrite = 0; //Start Fresh!
	//  Convert ODR
	switch(settings->accelSampleRate)
	{
		case 1:
		dataToWrite |= (0x01 << 4);
		break;
		case 10:
		dataToWrite |= (0x02 << 4);
		break;
		case 25:
		dataToWrite |= (0x03 << 4);
		break;
		case 50:
		dataToWrite |= (0x04 << 4);
		break;
		case 100:
		dataToWrite |= (0x05 << 4);
		break;
		case 200:
		dataToWrite |= (0x06 << 4);
		break;
		default:
		case 400:
		dataToWrite |= (0x07 << 4);
		break;
		case 1600:
		dataToWrite |= (0x08 << 4);
		break;
		case 5000:
		dataToWrite |= (0x09 << 4);
		break;
	}
	
	dataToWrite |= (settings->zAccelEnabled & 0x01) << 2;
	dataToWrite |= (settings->yAccelEnabled & 0x01) << 1;
	dataToWrite |= (settings->xAccelEnabled & 0x01);
	//Now, write the patched together data
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_CTRL_REG1(0x%02x) := 0x%02x\r\n", LIS3DH_CTRL_REG1, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_CTRL_REG1, dataToWrite);

	//Build CTRL_REG4
	dataToWrite = 0; //Start Fresh!
	//  Convert scaling
	switch(settings->accelRange)
	{
		case 2:
		dataToWrite |= (0x00 << 4);
		break;
		case 4:
		dataToWrite |= (0x01 << 4);
		break;
		case 8:
		dataToWrite |= (0x02 << 4);
		break;
		default:
		case 16:
		dataToWrite |= (0x03 << 4);
		break;
	}
	dataToWrite |= 0x80; //set block update
	dataToWrite |= 0x08; //set high resolution
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_CTRL_REG4(0x%02x) := 0x%02x\r\n", LIS3DH_CTRL_REG4, dataToWrite);
#endif
	//Now, write the patched together data
	Lis3dh_writeRegister(LIS3DH_CTRL_REG4, dataToWrite);
}

static void Lis3dh_config_Intterupts(void)
{
  uint8_t dataToWrite = 0;

  //LIS3DH_INT1_CFG   
  //dataToWrite |= 0x80;//AOI, 0 = OR 1 = AND
  //dataToWrite |= 0x40;//6D, 0 = interrupt source, 1 = 6 direction source
  //Set these to enable individual axes of generation source (or direction)
  // -- high and low are used generically
  //dataToWrite |= 0x20;//Z high
  //dataToWrite |= 0x10;//Z low
  dataToWrite |= 0x08;//Y high
  //dataToWrite |= 0x04;//Y low
  //dataToWrite |= 0x02;//X high
  //dataToWrite |= 0x01;//X low
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_INT1_CFG(0x%02x) := 0x%02x\r\n", LIS3DH_INT1_CFG, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_INT1_CFG, dataToWrite);
//  myIMU.writeRegister(LIS3DH_INT1_CFG, dataToWrite);
  
  //LIS3DH_INT1_THS   
  dataToWrite = 0;
  //Provide 7 bit value, 0x7F always equals max range by accelRange setting
  dataToWrite |= 0x10; // 1/8 range
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_INT1_THS(0x%02x) := 0x%02x\r\n", LIS3DH_INT1_THS, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_INT1_THS, dataToWrite);
//  myIMU.writeRegister(LIS3DH_INT1_THS, dataToWrite);
  
  //LIS3DH_INT1_DURATION  
  dataToWrite = 0;
  //minimum duration of the interrupt
  //LSB equals 1/(sample rate)
  dataToWrite |= 0x01; // 1 * 1/50 s = 20ms
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_INT1_DURATION(0x%02x) := 0x%02x\r\n", LIS3DH_INT1_DURATION, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_INT1_DURATION, dataToWrite);
//  myIMU.writeRegister(LIS3DH_INT1_DURATION, dataToWrite);
  
  //LIS3DH_CLICK_CFG   
  dataToWrite = 0;
  //Set these to enable individual axes of generation source (or direction)
  // -- set = 1 to enable
  //dataToWrite |= 0x20;//Z double-click
  dataToWrite |= 0x10;//Z click
  //dataToWrite |= 0x08;//Y double-click 
  dataToWrite |= 0x04;//Y click
  //dataToWrite |= 0x02;//X double-click
  dataToWrite |= 0x01;//X click
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_CLICK_CFG(0x%02x) := 0x%02x\r\n", LIS3DH_CLICK_CFG, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_CLICK_CFG, dataToWrite);
//  myIMU.writeRegister(LIS3DH_CLICK_CFG, dataToWrite);
  
  //LIS3DH_CLICK_SRC
  dataToWrite = 0;
  //Set these to enable click behaviors (also read to check status)
  // -- set = 1 to enable
  //dataToWrite |= 0x20;//Enable double clicks
  dataToWrite |= 0x04;//Enable single clicks
  //dataToWrite |= 0x08;//sine (0 is positive, 1 is negative)
  dataToWrite |= 0x04;//Z click detect enabled
  dataToWrite |= 0x02;//Y click detect enabled
  dataToWrite |= 0x01;//X click detect enabled
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_CLICK_SRC(0x%02x) := 0x%02x\r\n", LIS3DH_CLICK_SRC, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_CLICK_SRC, dataToWrite);
//  myIMU.writeRegister(LIS3DH_CLICK_SRC, dataToWrite);
  
  //LIS3DH_CLICK_THS   
  dataToWrite = 0;
  //This sets the threshold where the click detection process is activated.
  //Provide 7 bit value, 0x7F always equals max range by accelRange setting
  dataToWrite |= 0x0A; // ~1/16 range
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_CLICK_THS(0x%02x) := 0x%02x\r\n", LIS3DH_CLICK_THS, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_CLICK_THS, dataToWrite);
//  myIMU.writeRegister(LIS3DH_CLICK_THS, dataToWrite);
  
  //LIS3DH_TIME_LIMIT  
  dataToWrite = 0;
  //Time acceleration has to fall below threshold for a valid click.
  //LSB equals 1/(sample rate)
  dataToWrite |= 0x08; // 8 * 1/50 s = 160ms
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_TIME_LIMIT(0x%02x) := 0x%02x\r\n", LIS3DH_TIME_LIMIT, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_TIME_LIMIT, dataToWrite);
//  myIMU.writeRegister(LIS3DH_TIME_LIMIT, dataToWrite);
  
  //LIS3DH_TIME_LATENCY
  dataToWrite = 0;
  //hold-off time before allowing detection after click event
  //LSB equals 1/(sample rate)
  dataToWrite |= 0x08; // 4 * 1/50 s = 160ms
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_TIME_LATENCY(0x%02x) := 0x%02x\r\n", LIS3DH_TIME_LATENCY, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_TIME_LATENCY, dataToWrite);
//  myIMU.writeRegister(LIS3DH_TIME_LATENCY, dataToWrite);
  
  //LIS3DH_TIME_WINDOW 
  dataToWrite = 0;
  //hold-off time before allowing detection after click event
  //LSB equals 1/(sample rate)
  dataToWrite |= 0x10; // 16 * 1/50 s = 320ms
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_TIME_WINDOW(0x%02x) := 0x%02x\r\n", LIS3DH_TIME_WINDOW, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_TIME_WINDOW, dataToWrite);
//  myIMU.writeRegister(LIS3DH_TIME_WINDOW, dataToWrite);

  //LIS3DH_CTRL_REG5
  //Int1 latch interrupt and 4D on  int1 (preserve fifo en)
//  myIMU.readRegister(&dataToWrite, LIS3DH_CTRL_REG5);

  Lis3dh_readRegister(&dataToWrite, LIS3DH_CTRL_REG5);

  dataToWrite &= 0xF3; //Clear bits of interest
  dataToWrite |= 0x08; //Latch interrupt (Cleared by reading int1_src)
  //dataToWrite |= 0x04; //Pipe 4D detection from 6D recognition to int1?
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_CTRL_REG5(0x%02x) := 0x%02x\r\n", LIS3DH_CTRL_REG5, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_CTRL_REG5, dataToWrite);
//  myIMU.writeRegister(LIS3DH_CTRL_REG5, dataToWrite);

  //LIS3DH_CTRL_REG3
  //Choose source for pin 1
  dataToWrite = 0;
  //dataToWrite |= 0x80; //Click detect on pin 1
  dataToWrite |= 0x40; //AOI1 event (Generator 1 interrupt on pin 1)
  dataToWrite |= 0x20; //AOI2 event ()
  //dataToWrite |= 0x10; //Data ready
  //dataToWrite |= 0x04; //FIFO watermark
  //dataToWrite |= 0x02; //FIFO overrun
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_CTRL_REG3(0x%02x) := 0x%02x\r\n", LIS3DH_CTRL_REG3, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_CTRL_REG3, dataToWrite);
//  myIMU.writeRegister(LIS3DH_CTRL_REG3, dataToWrite);
 
  //LIS3DH_CTRL_REG6
  //Choose source for pin 2 and both pin output inversion state
  dataToWrite = 0;
  dataToWrite |= 0x80; //Click int on pin 2
  //dataToWrite |= 0x40; //Generator 1 interrupt on pin 2
  //dataToWrite |= 0x10; //boot status on pin 2
  //dataToWrite |= 0x02; //invert both outputs
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
	ESP_LOGI(TAG, "LIS3DH_CTRL_REG6(0x%02x) := 0x%02x\r\n", LIS3DH_CTRL_REG6, dataToWrite);
#endif
	Lis3dh_writeRegister(LIS3DH_CTRL_REG6, dataToWrite);
//  myIMU.writeRegister(LIS3DH_CTRL_REG6, dataToWrite);
  
}

//****************************************************************************//
//
//  Accelerometer section
//
//****************************************************************************//

static int16_t Lis3dh_readRawAccelX( void )
{
	int16_t output;
	Lis3dh_readRegisterInt16( &output, LIS3DH_OUT_X_L );
	return output;
}
static float Lis3dh_readFloatAccelX( void )
{
	float output = Lis3dh_calcAccel(Lis3dh_readRawAccelX());
	return output;
}

static int16_t Lis3dh_readRawAccelY( void )
{
	int16_t output;
	Lis3dh_readRegisterInt16( &output, LIS3DH_OUT_Y_L );
	return output;
}

static float Lis3dh_readFloatAccelY( void )
{
	float output = Lis3dh_calcAccel(Lis3dh_readRawAccelY());
	return output;
}

static int16_t Lis3dh_readRawAccelZ( void )
{
	int16_t output;
	Lis3dh_readRegisterInt16( &output, LIS3DH_OUT_Z_L );
	return output;

}

static float Lis3dh_readFloatAccelZ( void )
{
	float output = Lis3dh_calcAccel(Lis3dh_readRawAccelZ());
	return output;
}

static float Lis3dh_calcAccel( int16_t input )
{
	float output;
	switch(current_settings->accelRange)
	{
		case 2:
		output = (float)input / 15987;
		break;
		case 4:
		output = (float)input / 7840;
		break;
		case 8:
		output = (float)input / 3883;
		break;
		case 16:
		output = (float)input / 1280;
		break;
		default:
		output = 0;
		break;
	}
	return output;
}

//****************************************************************************//
//
//  Interrupt from accelerometer section
//
//****************************************************************************//

static void IRAM_ATTR lis3dh_gpio_isr_handler(void* arg)
{
//     portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
    AppMessage_t ISRMsg;   // Wakeup the app ..
	if (xAppInQueue != NULL) {
		gpio_intr_disable(LIS3DH_GPIO_INT_PIN);
		ISRMsg.cmd = APPMSG_LIS3DH_INTR;
		ISRMsg.d.v = (uint32_t) arg;
		ISRMsg.app__doneCallback = NULL;
    		xQueueSendFromISR(xAppInQueue, &ISRMsg,  NULL /*&xHigherPriorityTaskWoken*/);
	}
#if 0	
	/* THIS MUST BE THE LAST THING DONE IN THE ISR. */	
	if( xHigherPriorityTaskWoken )
	{
		taskYIELD();
	}
#endif
}

static void Lis3dh_gpio_cfg(void)
{
    gpio_config_t io_conf;

    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    //bit mask of the pins, use GPIO25 here
    io_conf.pin_bit_mask = (1<< LIS3DH_GPIO_INT_PIN);
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(LIS3DH_GPIO_INT_PIN, lis3dh_gpio_isr_handler, (void*) LIS3DH_GPIO_INT_PIN);

}


void Lis3dhIntrClean(void) 
{
  	uint8_t dataRead;

	Lis3dh_readRegister(&dataRead, LIS3DH_INT1_SRC);//cleared by reading
#ifdef CONFIG_LIS3DH_VERBOSE_DEBUG
  	printf("Decoded events: ");
  	if(dataRead & 0x40) printf("Interrupt Active ");
  	if(dataRead & 0x20) printf("Z high ");
  	if(dataRead & 0x10) printf("Z low ");
  	if(dataRead & 0x08) printf("Y high ");
  	if(dataRead & 0x04) printf("Y low ");
  	if(dataRead & 0x02) printf("X high ");
  	if(dataRead & 0x01) printf("X low");
  	printf("\r\n");
#endif
	gpio_intr_enable(LIS3DH_GPIO_INT_PIN);

}
