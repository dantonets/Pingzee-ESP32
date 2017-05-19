/*
 * task_spi.c
 *
 *  Created on: May 16, 2017
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


static const char *TAG = "SPI_master";

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18


#define spiQUEUE_LENGTH				( 4 )

static xQueueHandle xSPIMasterInQueue = NULL;
static xQueueHandle xSPIMasterOutQueue = NULL;
static xSemaphoreHandle xSPIMutex = NULL;

static void prvSPIMasterTask( void *pvParameters );

static pSPIMessage master_pSPIMsg = NULL;   // current transaction ..
static unsigned char SPI_init = 0;


static void spi_init(void)
{
    esp_err_t ret;
    spi_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,
        .mosi_io_num=PIN_NUM_MOSI,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };

    //Initialize the SPI bus
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    configASSERT(ret==ESP_OK);
}

static void device_init(spi_device_handle_t *spi, int cs)
{
    esp_err_t ret;
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=10000000,		//Clock out at 10 MHz
        .mode=0,						//SPI mode 0
        .spics_io_num=cs,				//CS pin
        .queue_size=3,					//We want to be able to queue 3 transactions at a time
        .pre_cb=NULL,					//Specify pre-transfer callback to handle D/C line (for example)
    };
    //Attach the device to the SPI bus
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, spi);
    configASSERT(ret==ESP_OK);
}

void vSPIMasterStart( uint16_t usStackSize, UBaseType_t uxPriority)
{
	spi_init();
	/* Create the queues used by tasks and interrupts to send/receive SPI data. */
	xSPIMasterInQueue = xQueueCreate( spiQUEUE_LENGTH, sizeof(SPIMessage_t) );
	xSPIMasterOutQueue = xQueueCreate( 1, sizeof(SPIMessage_t) );

	/* If the queue could not be created then don't create any tasks that might
	attempt to use the queue. */
	if( xSPIMasterInQueue != NULL && xSPIMasterOutQueue != NULL)
	{
	
		/* Create the semaphore used to access the SPI transmission. */
		xSPIMutex = xSemaphoreCreateMutex();
		configASSERT( xSPIMutex );
		/* Create that task that handles the I2C Master itself. */
		xTaskCreate(	prvSPIMasterTask,			/* The task that implements the command console. */
						(const char *const) "SPI",		/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
						usStackSize,					/* The size of the stack allocated to the task. */
						NULL,						/* The parameter is not used, so NULL is passed. */
						uxPriority,					/* The priority allocated to the task. */
						NULL );						/* A handle is not required, so just pass NULL. */
	}

}


/*-----------------------------------------------------------*/
static void prvSPIMasterTask( void *pvParameters )
{
	SPIMessage_t _SPIMsg;   // current transaction ..
	( void ) pvParameters;
	esp_err_t  ret;
	spi_transaction_t *rtrans;

		master_pSPIMsg = &_SPIMsg;
		while(1)
		{
			SPI_init = 1;
			if( xQueueReceive( xSPIMasterInQueue, master_pSPIMsg, portMAX_DELAY) == pdPASS)
			{
				if (master_pSPIMsg->app_msg.cmd)
				{
					switch (master_pSPIMsg->app_msg.cmd) {
						case APPMSG_SPI_ADD_DEVICE:
								device_init(&master_pSPIMsg->spi_dev, master_pSPIMsg->app_msg.d.v);
							break;
						case APPMSG_GET_STACK_ROOM:
								master_pSPIMsg->app_msg.d.v= uxTaskGetStackHighWaterMark(NULL);
							break;
					}
					if (master_pSPIMsg->app_msg.app__doneCallback != NULL)
						master_pSPIMsg->app_msg.app__doneCallback(master_pSPIMsg);
				}else{

					ret=spi_device_queue_trans(master_pSPIMsg->spi_dev, &master_pSPIMsg->trans, portMAX_DELAY);
					if (ret != ESP_OK) {
						VApplicationGeneralFault;
					}
					ret=spi_device_get_trans_result(master_pSPIMsg->spi_dev, &rtrans, portMAX_DELAY);
					if (ret != ESP_OK) {
						VApplicationGeneralFault;
					}

					if (master_pSPIMsg->spi__doneCallback != NULL)
					master_pSPIMsg->spi__doneCallback(master_pSPIMsg);
				}
			}
		}
}

/*-----------------------------------------------------------*/
portBASE_TYPE isSPIReady(void)
{
	if (SPI_init) 	return pdTRUE;
	return pdFALSE;
}
portBASE_TYPE SPIMsgPut(pSPIMessage msg)
{

	if(xQueueSend(xSPIMasterInQueue, msg, ( portTickType ) 10) != pdPASS)
		{
						VApplicationGeneralFault;
		}
	return pdPASS;
}

portBASE_TYPE SPIMsgGet(pSPIMessage msg)
{
	return xQueueReceive(xSPIMasterOutQueue, msg, portMAX_DELAY);
}

portBASE_TYPE  SPIOnDone(void *pvParameters)
{
	pSPIMessage msg = (pSPIMessage)pvParameters;

	if(xQueueSend(xSPIMasterOutQueue, msg, ( portTickType ) (10 /portTICK_RATE_MS)) != pdPASS)
	{
			VApplicationGeneralFault;
	}
		
	return pdPASS;
}	
portBASE_TYPE SPITake(void *pvParameters)
{
	( void ) pvParameters;
	if (xSPIMutex == NULL)  return pdFAIL;

        // Obtain the semaphore - don't block even if the semaphore is not
        // available during a time more then a reasonable.
	if( xSemaphoreTake( xSPIMutex, (portMAX_DELAY -10) )  != pdPASS )
	{
		// Never return from  ..
		{
			VApplicationGeneralFault;
		}
	}
	return pdPASS;
}
portBASE_TYPE SPIGive(void *pvParameters)
{
	( void ) pvParameters;
	if (xSPIMutex == NULL)  return pdFAIL;

	if( xSemaphoreGive( xSPIMutex ) != pdPASS )
	{
		// Never return from  ..
		{
			VApplicationGeneralFault;
		}
	}
	return pdPASS;
}
/*-----------------------------------------------------------*/

