/*
 * app.h
 *
 *  Created on: May 11, 2017
 *      Author: beam
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "driver/spi_master.h"
#include "esp_deep_sleep.h"
#include "board.h"

#ifndef MAIN_APP_H_
#define MAIN_APP_H_

//****************************************************************************//
//
//  Application interface
//
//****************************************************************************//

// Global state for the Application task.
/**
 * @brief Application status at end of "wake" state
 */
typedef enum {
    WAKE_NOTHING			= 0, /*!<Nothing happen durint the "wake" state*/
    WAKE_BT_RESPONDED		= 1, /*!<BT has a respond for advertising*/
    WAKE_MANUALLY_STOPED	= 2, /*!<Going to debugging mode, without deep sleep*/
    WAKE_MAX,
} at_wake_end_t;

#define APP_DEEP_SLEEP_PERIOD		60
#define APP_WAKE_PERIOD			2

typedef struct {
	struct timeval						app_time;            	// Last time when something changes in the status ..
	esp_deep_sleep_wakeup_cause_t		wakeup_reason;
	char 							*tz;					// TimeZone, setups from Konfig	
	at_wake_end_t					what_happen; 
} AppState_t, *pAppState;

#if (CONFIG_APPLICATION_DEBUG)
void vApplicationGeneralFault( const char *function_name, int line);
#define  VApplicationGeneralFault	vApplicationGeneralFault(__FUNCTION__, __LINE__)
#else
void vApplicationGeneralFault(void);
#define  VApplicationGeneralFault	vApplicationGeneralFault()
#endif

void vAppSetCurrentTime(void); 
void vAppWakeup(pAppState  st);
pAppState psAppGetStatus(void);


#define APPMSG_NOP						(0x00)
#define APPMSG_GET_STACK_ROOM		(0x01)
#define APPMSG_SPI_ADD_DEVICE			(0x02)
#define APPMSG_OLED_INIT				(0x03)
#define APPMSG_OLED_CLEAN				(0x04)
#define APPMSG_OLED_SHOW				(0x05)
#define APPMSG_LIS3DH_CHECK_PRESENT	(0x06)
#define APPMSG_LIS3DH_SETUP			(0x07)
#define APPMSG_LIS3DH_INTR				(0x08)
#define APPMSG_TIMER_INIT				(0x09)
#define APPMSG_TIMER_INTR				(0x0A)
#define APPMSG_TIMER_START			(0x0B)
#define APPMSG_TIMER_STOP				(0x0C)
#define APPMSG_MAIN_LOOP_START		(0x0D)
#define APPMSG_MAIN_LOOP_STOP		(0x0E)
#define APPMSG_DEEP_SLEEP				(0x0F)

typedef struct {
	uint8_t	cmd;
	union {
		uint32_t	v;
		uint8_t	param[4];
		void *	ptr;
	} d;
	portBASE_TYPE (*app__doneCallback)(void *);
} AppMessage_t, *pAppMessage;

extern xQueueHandle xAppInQueue;
void vAppStart( uint16_t usStackSize, portBASE_TYPE uxPriority );
portBASE_TYPE AppMsgPut(pAppMessage msg);
portBASE_TYPE  AppOnDone(void *pvParameters);
portBASE_TYPE AppMsgGet(pAppMessage msg);
portBASE_TYPE AppTake(void *pvParameters);
portBASE_TYPE AppGive(void *pvParameters);


//****************************************************************************//
//
//  I2C transactions
//
//****************************************************************************//


/**
 * \brief Information concerning the data transmission.
 */
typedef struct i2c_packet {
	//! I2C address/commands to issue to the other chip (node).
	uint8_t addr[3];
	//! Length of the I2C data address segment (1-3 bytes).
	uint32_t addr_length;
	//! Where to find the data to be transferred.
	void *buffer;
	//! How many bytes do we want to transfer.
	size_t  length;
	//! I2C chip address to communicate with.
	uint8_t chip;
} i2c_packet_t;

typedef struct {
	i2c_packet_t	tx;
	i2c_packet_t	rx;
	portTickType  tx_wait_time;    // It can be a long time for write operatuions (for EEPROM, for example)
	portBASE_TYPE (*i2c__doneCallback)(void *);
} I2CMessage_t, *pI2CMessage;

portBASE_TYPE isI2C0Ready(void);
portBASE_TYPE I2C0MsgPut(pI2CMessage msg);
portBASE_TYPE I2C0MsgGet(pI2CMessage msg);
portBASE_TYPE  I2C0OnDone(void *pvParameters);
portBASE_TYPE I2C0Take(void *pvParameters);
portBASE_TYPE I2C0Give(void *pvParameters);
void vI2C0MasterStart( uint16_t usStackSize, UBaseType_t uxPriority);

//****************************************************************************//
//
//  SPI transactions
//
//****************************************************************************//


typedef struct {
	AppMessage_t		app_msg;
	spi_device_handle_t spi_dev;
	spi_transaction_t	trans;
	portBASE_TYPE (*spi__doneCallback)(void *);
} SPIMessage_t, *pSPIMessage;

portBASE_TYPE isSPIReady(void);
portBASE_TYPE SPIMsgPut(pSPIMessage msg);
portBASE_TYPE SPIMsgGet(pSPIMessage msg);
portBASE_TYPE  SPIOnDone(void *pvParameters);
portBASE_TYPE SPITake(void *pvParameters);
portBASE_TYPE SPIGive(void *pvParameters);

void vSPIMasterStart( uint16_t usStackSize, UBaseType_t uxPriority);

//****************************************************************************//
//
//  OLED transactions
//
//****************************************************************************//


typedef struct {
	AppMessage_t		app_msg;
	unsigned char x;
	unsigned char y;
	unsigned short strLen;
	unsigned char *str;
	portBASE_TYPE (*oled__doneCallback)(void *);
} OledMessage_t, *pOledMessage;

portBASE_TYPE OledMsgPut(pOledMessage msg);
portBASE_TYPE isOledReady(void) ;
portBASE_TYPE  OledOnDone(void *pvParameters);
portBASE_TYPE OledMsgGet(pOledMessage msg);

void vOledClean(void);
void vOledStart( uint16_t usStackSize, portBASE_TYPE uxPriority );
void vOledWrite(unsigned char y, unsigned char x, unsigned char *str);
void vOledWriteRaw(unsigned char y, unsigned char x, unsigned char *str, unsigned short len);

//****************************************************************************//
//
//  Accelerometer transactions
//
//****************************************************************************//


typedef struct {
	AppMessage_t		app_msg;
	float				accl;
	portBASE_TYPE (*lis3dh__doneCallback)(void *);
} Lis3dhMessage_t, *pLis3dhMessage;

void vLis3dhStart( uint16_t usStackSize, portBASE_TYPE uxPriority );
portBASE_TYPE Lis3dhMsgPut(pLis3dhMessage msg);
portBASE_TYPE isLis3dhPresents(void);
portBASE_TYPE  Lis3dhOnDone(void *pvParameters);
portBASE_TYPE Lis3dhMsgGet(pLis3dhMessage msg);
void Lis3dhIntr1Clean(void);
void Lis3dhIntr2Clean(void);

//****************************************************************************//
//
//  Timers app control
//
//****************************************************************************//


typedef struct {
	AppMessage_t		app_msg;
	int				timer_idx;
	portBASE_TYPE (*apptimer__doneCallback)(void *);
} AppTimerCntl_t, *pAppTimerCntl;

portBASE_TYPE AppTimerMsgPut(pAppTimerCntl msg);
portBASE_TYPE  AppTimerOnDone(void *pvParameters);
portBASE_TYPE AppTimerMsgGet(pAppTimerCntl msg);
portBASE_TYPE  AppTimer0OnDone(void *pvParameters);
portBASE_TYPE AppTimer0MsgGet(pAppTimerCntl msg);
portBASE_TYPE AppTimer0MsgGetTimeout(pAppTimerCntl msg, TickType_t delay);
portBASE_TYPE  AppTimer1OnDone(void *pvParameters);
portBASE_TYPE AppTimer1MsgGet(pAppTimerCntl msg);
portBASE_TYPE  AppTimer2OnDone(void *pvParameters);
portBASE_TYPE AppTimer2MsgGet(pAppTimerCntl msg);
portBASE_TYPE  AppTimer3OnDone(void *pvParameters);
portBASE_TYPE AppTimer3MsgGet(pAppTimerCntl msg);
portBASE_TYPE AppTimersTake(void *pvParameters);
portBASE_TYPE AppTimersGive(void *pvParameters);
int getAppTimerVal4Secs(int secs);
void getAppTimerVal(int timer_idx, uint64_t* timer_val);
void vAppTimersStart( uint16_t usStackSize, portBASE_TYPE uxPriority );



#endif /* MAIN_APP_H_ */
