/*
 * ssd1306.c
 *
 *
 * \brief SSD1306 display controller driver.
 *
 *  Created on: May 17, 2017
 *      Author: beam
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "app.h"
#include "board.h"
#include "ssd1306.h"
#include "font.h"

static const char *TAG = "ssd1306";

static spi_device_handle_t ssd1306_dev = NULL;

#define oledQUEUE_LENGTH			( 4 )
#define OLED_NOT_INIT				(0x00)
#define OLED_INIT					(0x01)
#define OLED_OFF					(0x80)


static xQueueHandle xOledInQueue = NULL;
static xQueueHandle xOledOutQueue = NULL;
static xSemaphoreHandle xOledMutex = NULL;

static pOledMessage master_pOledMsg = NULL;   // current transaction ..
static uint8_t ucOledStatus = OLED_NOT_INIT;
static void prvOledTask( void *pvParameters );

/**
 * \internal
 * \brief Initialize the hardware interface
 *
 * Depending on what interface used for interfacing the OLED controller this
 * function will initialize the necessary hardware.
 */
static void ssd1306_interface_init(void)
{

	SPIMessage_t msg; 


	msg.app_msg.cmd = APPMSG_SPI_ADD_DEVICE;
	msg.app_msg.d.v = SSD1306_PIN_NUM_CS;
	msg.app_msg.app__doneCallback = SPIOnDone;
	SPITake(NULL);

	SPIMsgPut(&msg);
	while (SPIMsgGet(&msg) != pdFAIL) {
		break;
	}

	ssd1306_dev = msg.spi_dev;

ESP_LOGI(TAG, "device added, handle@0x%x\n", (uint32_t)ssd1306_dev);

	SPIGive(NULL);
}

void ssd1306_spi_write_single(uint8_t data)
{
	SPIMessage_t  msg, rc;

	msg.spi__doneCallback = SPIOnDone; // default ..
	msg.spi_dev = ssd1306_dev;
       memset(&msg.trans, 0, sizeof(spi_transaction_t));
	msg.trans.length = 8;
	msg.trans.user = NULL;
	msg.trans.flags=SPI_TRANS_USE_TXDATA;
	msg.trans.tx_data[0] = data;
	msg.app_msg.cmd = APPMSG_NOP;

//	 spiTRFTake(NULL);
	 while (SPIMsgPut(&msg) != pdFAIL) {
	 	break;
	 }
	 
	 while (SPIMsgGet(&rc) != pdFAIL) {
		break;
	 }
//	 spiTRFGive(NULL);
}

void ssd1306_spi_write(uint8_t *data, unsigned short len)
{
	SPIMessage_t  msg, rc;

	msg.spi__doneCallback = SPIOnDone; // default ..
	msg.spi_dev = ssd1306_dev;
       memset(&msg.trans, 0, sizeof(spi_transaction_t));
	msg.trans.length = (len*8);
	msg.trans.rxlength = 0;
	msg.trans.user = NULL;
	msg.trans.flags= 0;
	msg.trans.tx_buffer = data;
	msg.trans.rx_buffer = NULL;
	msg.app_msg.cmd = APPMSG_NOP;

//	 spiTRFTake(NULL);
	 while (SPIMsgPut(&msg) != pdFAIL) {
	 	break;
	 }
	 
	 while (SPIMsgGet(&rc) != pdFAIL) {
		break;
	 }
//	 spiTRFGive(NULL);
}

/**
 * \brief Initialize the OLED controller
 *
 * Call this function to initialize the hardware interface and the OLED
 * controller. When initialization is done the display is turned on and ready
 * to receive data.
 */
void ssd1306_init(void)
{

	// Do a hard reset of the OLED display controller
	ssd1306_hard_reset();

	// Initialize the interface
	ssd1306_interface_init();


	// 1/32 Duty (0x0F~0x3F)
	ssd1306_write_command(SSD1306_CMD_SET_MULTIPLEX_RATIO);
	ssd1306_write_command(0x1F);

	// Shift Mapping RAM Counter (0x00~0x3F)
	ssd1306_write_command(SSD1306_CMD_SET_DISPLAY_OFFSET);
	ssd1306_write_command(0x00);

	// Set Mapping RAM Display Start Line (0x00~0x3F)
	ssd1306_write_command(SSD1306_CMD_SET_START_LINE(0x00));

	// Set Column Address 0 Mapped to SEG0
	ssd1306_write_command(SSD1306_CMD_SET_SEGMENT_RE_MAP_COL127_SEG0);

	// Set COM/Row Scan Scan from COM63 to 0
	ssd1306_write_command(SSD1306_CMD_SET_COM_OUTPUT_SCAN_DOWN);

	// Set COM Pins hardware configuration
	ssd1306_write_command(SSD1306_CMD_SET_COM_PINS);
	ssd1306_write_command(0x02);

	ssd1306_set_contrast(0x8F);

	// Disable Entire display On
	ssd1306_write_command(SSD1306_CMD_ENTIRE_DISPLAY_AND_GDDRAM_ON);

	ssd1306_display_invert_disable();

	// Set Display Clock Divide Ratio / Oscillator Frequency (Default => 0x80)
	ssd1306_write_command(SSD1306_CMD_SET_DISPLAY_CLOCK_DIVIDE_RATIO);
	ssd1306_write_command(0x80);

	// Enable charge pump regulator
	ssd1306_write_command(SSD1306_CMD_SET_CHARGE_PUMP_SETTING);
	ssd1306_write_command(0x14);

	// Set VCOMH Deselect Level
	ssd1306_write_command(SSD1306_CMD_SET_VCOMH_DESELECT_LEVEL);
	ssd1306_write_command(0x40); // Default => 0x20 (0.77*VCC)

	// Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
	ssd1306_write_command(SSD1306_CMD_SET_PRE_CHARGE_PERIOD);
	ssd1306_write_command(0xF1);

	ssd1306_display_on();
}

static /* inline */ void ssd1306_clear(void)
{
	uint8_t page = 0;
	uint8_t col = 0;
	

	for (page = 0; page < 4; ++page)
	{
		ssd1306_set_page_address(page);
		ssd1306_set_column_address(0);
		for (col = 0; col < 128; ++col)
		{
			ssd1306_write_data(0x00);
		}
	}
}

/**
 * \brief Display text on OLED screen.
 * \param string String to display.
 */
void ssd1306_write_text(const char *string)
{
	uint8_t *char_ptr;
	uint8_t i;

	while (*string != 0) {
		if (*string < 0x7F) {
			char_ptr = (uint8_t *)font_table[*string - 32];
			for (i = 1; i <= char_ptr[0]; i++) {
				ssd1306_write_data(char_ptr[i]);
			}
			ssd1306_write_data(0x20);
		}
			string++;
	}
}

/**
 * \brief Write bytes to the OLED screen.
 * \param bytes bytes to display.
 * \param len  number bytes to display.
 */
void ssd1306_write_bytes(const uint8_t *bytes, unsigned short len)
{
	ssd1306_spi_write((uint8_t *)bytes, len);
}

/*-----------------------------------------------------------*/

void vOledStart( uint16_t usStackSize, portBASE_TYPE uxPriority )
{
	/* Create the queues used by tasks and interrupts to send/receive SPI data. */
	xOledInQueue = xQueueCreate( oledQUEUE_LENGTH, sizeof(OledMessage_t) );
	xOledOutQueue = xQueueCreate( 2, sizeof(OledMessage_t) );

	/* If the queue could not be created then don't create any tasks that might
	attempt to use the queue. */
	if( xOledInQueue != NULL && xOledOutQueue != NULL)
	{
	
		/* Create the semaphore used to access the SPI transmission. */
		xOledMutex = xSemaphoreCreateMutex();
		configASSERT( xOledMutex );

		/* Create that task that handles the I2C Master itself. */
		xTaskCreate(prvOledTask,	/* The task that implements the command console. */
					(const char *const) "OLED",		/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
					usStackSize,					/* The size of the stack allocated to the task. */
					NULL,						/* The parameter is not used, so NULL is passed. */
					uxPriority,					/* The priority allocated to the task. */
					NULL );						/* A handle is not required, so just pass NULL. */

	}
}

/*-----------------------------------------------------------*/
static void prvOledTask( void *pvParameters )
{
	OledMessage_t _OledMsg;   // current transaction ..
	( void ) pvParameters;

		master_pOledMsg = &_OledMsg;
		_OledMsg.app_msg.cmd = APPMSG_OLED_INIT;
		master_pOledMsg->app_msg.app__doneCallback = NULL;
		master_pOledMsg->oled__doneCallback = NULL;

		vTaskDelay((portTickType)(250 / portTICK_RATE_MS));
		
		while(!isSPIReady()) {
			vTaskDelay(100);
		}

		vTaskDelay((portTickType)(250 / portTICK_RATE_MS));
		
		OledMsgPut(master_pOledMsg);

		vTaskDelay((portTickType)(250 / portTICK_RATE_MS));
		
		while(1)
		{	
			if( xQueueReceive( xOledInQueue, master_pOledMsg, portMAX_DELAY) == pdPASS)
			{
				switch (master_pOledMsg ->app_msg.cmd)
				{
					case APPMSG_GET_STACK_ROOM:
							master_pOledMsg->app_msg.d.v= uxTaskGetStackHighWaterMark(NULL);
							if (master_pOledMsg->app_msg.app__doneCallback != NULL)
								master_pOledMsg->app_msg.app__doneCallback(master_pOledMsg);
						break;
					case APPMSG_OLED_INIT: 
						if (isSPIReady()) {		
							ssd1306_init();
							ucOledStatus = OLED_INIT;
						}else{
							// [ADK] Temporary, until we will have ALL tasks implemented
							VApplicationGeneralFault;
						}
						
						// Might as well clear too
						// ssd1306_clear();
						
						break;
					case APPMSG_OLED_CLEAN: 
						 vOledWriteRaw(master_pOledMsg->y, master_pOledMsg->x, master_pOledMsg->str, master_pOledMsg->strLen);
						break;
					case APPMSG_OLED_SHOW:
						 vOledWrite(master_pOledMsg->y, master_pOledMsg->x, master_pOledMsg->str);
						break;
				}
			}
			if (master_pOledMsg->oled__doneCallback)
				master_pOledMsg->oled__doneCallback((void *)master_pOledMsg);
			
		}
}
/*-----------------------------------------------------------*/

portBASE_TYPE OledMsgPut(pOledMessage msg)
{
	if(xQueueSend(xOledInQueue, msg, ( portTickType ) 10) != pdPASS)
		{
			VApplicationGeneralFault;
		}
	return pdPASS;
}

portBASE_TYPE isOledReady(void) 
{
	if (ucOledStatus == OLED_INIT)  return pdTRUE;
	return pdFALSE;
}

/*-----------------------------------------------------------*/
portBASE_TYPE  OledOnDone(void *pvParameters)
{
	pOledMessage msg = (pOledMessage)pvParameters;

	if(xQueueSend(xOledOutQueue, msg, ( portTickType ) (10 /portTICK_RATE_MS)) != pdPASS)
	{
		VApplicationGeneralFault;
	}
		
	return pdPASS;
}	
portBASE_TYPE OledMsgGet(pOledMessage msg)
{
	return xQueueReceive(xOledOutQueue, msg, portMAX_DELAY);
}

/*-----------------------------------------------------------*/
void vOledClean(void)
{
	OledMessage_t _OledMsg;   // current transaction ..
	_OledMsg.app_msg.cmd = APPMSG_OLED_CLEAN;
	OledMsgPut(&_OledMsg);
}


void OledDisplay(unsigned char Row, unsigned char Col, unsigned char *Str)
{
	OledMessage_t _OledMsg;

	_OledMsg.app_msg.cmd = APPMSG_OLED_SHOW;
	_OledMsg.y = Row;
	_OledMsg.x = Col;
	_OledMsg.str = Str;
	_OledMsg.strLen = strlen((const char *)Str);
	_OledMsg.oled__doneCallback = OledOnDone;

	OledMsgPut(&_OledMsg);
	while(OledMsgGet(&_OledMsg) != pdFAIL) {break;}
}

void vOledWrite(unsigned char y, unsigned char x, unsigned char *str)
{
	ssd1306_set_page_address(y);
	ssd1306_set_column_address(x);
	ssd1306_write_text((const char *)str);
}

void vOledWriteRaw(unsigned char y, unsigned char x, unsigned char *str, unsigned short len)
{
	ssd1306_sel_cmd();
	ssd1306_set_page_address(y);
	ssd1306_set_column_address(x);
	ssd1306_sel_data();	
	ssd1306_write_bytes((const uint8_t *)str, len);
}

