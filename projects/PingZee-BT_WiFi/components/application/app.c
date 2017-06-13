/*
 * app.c
 *
 *  Created on: May 12, 2017
 *      Author: beam
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/rtc_io.h"
#include "soc/rtc_cntl_reg.h"
#include "FreeRTOS_CLI.h"
#include "app.h"


#define AppQUEUE_LENGTH	3

static const char *APP_TAG = "App";

static AppState_t  sAppState;

xQueueHandle xAppInQueue = NULL;      // Make it avaliable for the ISR handlers ..
xQueueHandle xAppOutQueue = NULL;
static xSemaphoreHandle xAppMutex = NULL;
static pAppMessage master_pAppMsg = NULL;   // current transaction ..

static RTC_DATA_ATTR struct timeval sleep_enter_time;

static void prvAppTask( void *pvParameters );
static void vAppMainLoopStart(pAppMessage pAppMsg);
static void vAppDeepSleep(pAppMessage pAppMsg); 
static void prvAppTimerExpieredTask( void *pvParameters );


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
		
		sAppState.what_happen = WAKE_NOTHING;


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
							if (master_pAppMsg->d.v == LIS3DH_GPIO_INT_PIN1)	
								Lis3dhIntr1Clean();
							else
							if (master_pAppMsg->d.v == LIS3DH_GPIO_INT_PIN2)	
								Lis3dhIntr2Clean();
						break;
					case APPMSG_MAIN_LOOP_START:
						sAppState.what_happen = WAKE_NOTHING;
						vAppMainLoopStart(master_pAppMsg);
						break;
					case APPMSG_MAIN_LOOP_STOP:
						sAppState.what_happen = WAKE_MANUALLY_STOPED;
						break;
					case APPMSG_DEEP_SLEEP:
						vAppDeepSleep(master_pAppMsg);
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
static void prvAppDeepSleepTimerTask( void *pvParameters )
{
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


pAppState psAppGetStatus(void) 
{
    	return &sAppState;
}

void vAppSetCurrentTime(void) 
{
    	gettimeofday(&sleep_enter_time, NULL);
}

void vAppWakeup(pAppState  st)
{
//    struct timeval now;
    gettimeofday(&st->app_time, NULL);
    int sleep_time_ms = (st->app_time.tv_sec - sleep_enter_time.tv_sec) * 1000 + (st->app_time.tv_usec - sleep_enter_time.tv_usec) / 1000;

    switch ((st->wakeup_reason = esp_deep_sleep_get_wakeup_cause())) {
        case ESP_DEEP_SLEEP_WAKEUP_EXT1: {
            uint64_t wakeup_pin_mask = esp_deep_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask != 0) {
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                printf("Wake up from GPIO %d\n", pin);
            } else {
                printf("Wake up from GPIO\n");
            }
            break;
        }
        case ESP_DEEP_SLEEP_WAKEUP_TIMER: {
            printf("Wake up from timer. Time spent in deep sleep: %dms\n", sleep_time_ms);
            break;
        }
        case ESP_DEEP_SLEEP_WAKEUP_UNDEFINED:
        default:
            printf("Not a deep sleep reset\n");
    }
	
}


static void vAppMainLoopStart(pAppMessage pAppMsg)
{
	AppTimerCntl_t msg;

	msg.timer_idx = 0;     // Use TIMER_GROUP_0, TIMER_0
	msg.app_msg.cmd = APPMSG_TIMER_INIT;
	msg.app_msg.d.v = getAppTimerVal4Secs(APP_WAKE_PERIOD);
	msg.app_msg.app__doneCallback = AppTimerOnDone;
	msg.apptimer__doneCallback = NULL;    // Use default handler for the "timer expiered" event..

	AppTimersTake(NULL);

	AppTimerMsgPut(&msg);

	while (AppTimerMsgGet(&msg) != pdFAIL) {
		break;
	}
	AppTimersGive(NULL);

	xTaskCreate(prvAppTimerExpieredTask,	/* The task that implements the application. */
				(const char *const) "ATi",		/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
				2048,						/* The size of the stack allocated to the task. */
				NULL,						/* The parameter is not used, so NULL is passed. */
				12,							/* The priority allocated to the task. */
				NULL );						/* A handle is not required, so just pass NULL. */


	msg.app_msg.cmd = APPMSG_TIMER_START;

	AppTimersTake(NULL);

	AppTimerMsgPut(&msg);

	while (AppTimerMsgGet(&msg) != pdFAIL) {
		break;
	}
	AppTimersGive(NULL);

}

static void prvAppTimerExpieredTask( void *pvParameters )
{
	AppTimerCntl_t msg;
	AppMessage_t dmsg;
//	uint64_t  val;

	while(1) {
		if (AppTimer0MsgGetTimeout(&msg, (200 / portTICK_PERIOD_MS)) == pdTRUE) 
		{
			ESP_LOGI(APP_TAG, "%s: got interrupt from timer ..\r\n", __func__);
			msg.timer_idx = 0;     // Use TIMER_GROUP_0, TIMER_0
			msg.app_msg.cmd = APPMSG_TIMER_STOP;
			msg.app_msg.d.v = getAppTimerVal4Secs(APP_WAKE_PERIOD);
			msg.app_msg.app__doneCallback = AppTimerOnDone;
			msg.apptimer__doneCallback = NULL;    // Use default handler for the "timer expiered" event..

			AppTimersTake(NULL);

			AppTimerMsgPut(&msg);

			while (AppTimerMsgGet(&msg) != pdFAIL) {
				break;
			}
			AppTimersGive(NULL);

			if (sAppState.what_happen == WAKE_NOTHING) 
			{
				// Nothing happen during the "wake" period, going to sleep again.. 
				dmsg.cmd = APPMSG_DEEP_SLEEP;
				dmsg.app__doneCallback = NULL;
				dmsg.d.v = APP_DEEP_SLEEP_PERIOD;
				AppMsgPut(&dmsg);
			}
			vTaskDelete(NULL);
		}
//		getAppTimerVal(0, &val);
//		printf("%s: h=%d, l=%d\r\n", __func__, (uint32_t)(val >> 32), (uint32_t)val&0xffffffff);
	}
	
}


static void vAppDeepSleep(pAppMessage pAppMsg) 
{

    	ESP_LOGI(APP_TAG, "Enabling timer wakeup, %ds\n", pAppMsg->d.v);
    	esp_deep_sleep_enable_timer_wakeup(pAppMsg->d.v * 1000000);

	const int ext_wakeup_pin_1 = 25;
	const uint64_t ext_wakeup_pin_1_mask = 1ULL << ext_wakeup_pin_1;
	
//TODO:	[ADK]	06/12/2017	not yet, need init LIS3DH	Lis3dhIntr1Clean();

	ESP_LOGI(APP_TAG, "Enabling EXT1 wakeup on pins GPIO%d\n", ext_wakeup_pin_1);
	esp_deep_sleep_enable_ext1_wakeup(ext_wakeup_pin_1_mask /*| ext_wakeup_pin_2_mask */, ESP_EXT1_WAKEUP_ANY_HIGH);

    	ESP_LOGI(APP_TAG, "Entering deep sleep\n");
	vAppSetCurrentTime();
	
    	esp_deep_sleep_start();

}
