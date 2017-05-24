/*
 * app.c
 *
 *  Created on: May 12, 2017
 *      Author: beam
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "FreeRTOS_CLI.h"
#include "app.h"


#define AppQUEUE_LENGTH	3

static const char *APP_TAG = "App";

xQueueHandle xAppInQueue = NULL;      // Make it avaliable for the ISR handlers ..
xQueueHandle xAppOutQueue = NULL;
static xSemaphoreHandle xAppMutex = NULL;
static pAppMessage master_pAppMsg = NULL;   // current transaction ..

static void prvAppTask( void *pvParameters );


/*-----------------------------------------------------------*/

void vAppStart( uint16_t usStackSize, portBASE_TYPE uxPriority )
{
	/* Create the queues used by application  tasks and interrupts */
	xAppInQueue = xQueueCreate( AppQUEUE_LENGTH, sizeof(AppMessage_t) );
	xAppOutQueue = xQueueCreate( 1, sizeof(AppMessage_t) );

	/* If the queue could not be created then don't create any tasks that might
	attempt to use the queue. */
	if( xAppInQueue != NULL && xAppOutQueue != NULL)
	{
	
		/* Create the semaphore used to access the SPI transmission. */
		xAppMutex = xSemaphoreCreateMutex();
		configASSERT( xAppMutex );

		/* Create that task that handles the I2C Master itself. */
		xTaskCreate(prvAppTask,					/* The task that implements the application. */
					(const char *const) "APP",		/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
					usStackSize,					/* The size of the stack allocated to the task. */
					NULL,						/* The parameter is not used, so NULL is passed. */
					uxPriority,					/* The priority allocated to the task. */
					NULL );						/* A handle is not required, so just pass NULL. */

	}
}



/*-----------------------------------------------------------*/
static void prvAppTask( void *pvParameters )
{
	AppMessage_t _AppMsg;   // current transaction ..
	( void ) pvParameters;

		master_pAppMsg = &_AppMsg;

		while(!isI2C0Ready()) {
			vTaskDelay(100);
		}

		while(1)
		{	
			if( xQueueReceive( xAppInQueue, master_pAppMsg, portMAX_DELAY) == pdPASS)
			{
				if (master_pAppMsg ->cmd) {
					switch (master_pAppMsg ->cmd)
					{
					case APPMSG_GET_STACK_ROOM:
							master_pAppMsg->d.v= uxTaskGetStackHighWaterMark(NULL);
						break;
					case APPMSG_LIS3DH_INTR:
						//Got interrupt from the accelerometer ..
							Lis3dhIntrClean();
						break;
					}
					if (master_pAppMsg->app__doneCallback != NULL)
						master_pAppMsg->app__doneCallback(master_pAppMsg);
				}
			}
		}
}

portBASE_TYPE AppMsgPut(pAppMessage msg)
{
	if(xQueueSend(xAppInQueue, msg, ( portTickType ) 10) != pdPASS)
		{
			VApplicationGeneralFault;
		}
	return pdPASS;
}

/*-----------------------------------------------------------*/
portBASE_TYPE  AppOnDone(void *pvParameters)
{
	pAppMessage msg = (pAppMessage)pvParameters;

	if(xQueueSend(xAppOutQueue, msg, ( portTickType ) (10 /portTICK_RATE_MS)) != pdPASS)
	{
		VApplicationGeneralFault;
	}
		
	return pdPASS;
}	
portBASE_TYPE AppMsgGet(pAppMessage msg)
{
	return xQueueReceive(xAppOutQueue, msg, portMAX_DELAY);
}

portBASE_TYPE AppTake(void *pvParameters)
{
	( void ) pvParameters;
	if (xAppMutex == NULL)  return pdFAIL;

        // Obtain the semaphore - don't block even if the semaphore is not
        // available during a time more then a reasonable.
	if( xSemaphoreTake( xAppMutex, (portMAX_DELAY -10) )  != pdPASS )
	{
		// Never return from  ..
		{
			VApplicationGeneralFault;
		}
	}
	return pdPASS;
}
portBASE_TYPE AppGive(void *pvParameters)
{
	( void ) pvParameters;
	if (xAppMutex == NULL)  return pdFAIL;

	if( xSemaphoreGive( xAppMutex ) != pdPASS )
	{
		// Never return from  ..
		{
			VApplicationGeneralFault;
		}
	}
	return pdPASS;
}


/*-----------------------------------------------------------*/


#if (CONFIG_APPLICATION_DEBUG)
void vApplicationGeneralFault(  const char *function_name, int line )
{
	ESP_LOGI(APP_TAG, "Stopped at %s, line %d\r\n", function_name, line);
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
#else
void vApplicationGeneralFault(void)
{
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
#endif


