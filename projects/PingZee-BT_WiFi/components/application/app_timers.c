/*
 * app_timers.c
 *
 *  Created on: Jun 6, 2017
 *      Author: beam
 */

#include <esp_types.h>
#include <stdio.h>
#include "rom/ets_sys.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/xtensa_api.h"
#include "soc/uart_reg.h"
#include "soc/dport_reg.h"
#include "soc/io_mux_reg.h"
#include "esp_intr_alloc.h"
#include "driver/timer.h"
#include "FreeRTOS_CLI.h"
#include "app.h"

#define AppTimersQUEUE_LENGTH	2
#define TIMER_DIVIDER   16               /*!< Hardware timer clock divider */
#define TIMER_SCALE    (TIMER_BASE_CLK / TIMER_DIVIDER)  /*!< used to calculate counter value */

static const char *TIMERS_TAG = "Timers";

static xQueueHandle xAppTimersInQueue = NULL;      
static xQueueHandle xAppTimersOutQueue = NULL;

// Timer IRQs events loaded to the queues:
static xQueueHandle xAppTimer00OutQueue = NULL;  // for index = 0
static xQueueHandle xAppTimer01OutQueue = NULL;  // for index = 1
static xQueueHandle xAppTimer10OutQueue = NULL;  // for index = 2
static xQueueHandle xAppTimer11OutQueue = NULL;  // for index = 3

static xSemaphoreHandle xAppTimersMutex = NULL;
static pAppTimerCntl master_pAppTimerMsg = NULL;   // current transaction ..

static timer_isr_handle_t inth[4];
static portBASE_TYPE (*fn__Callback[4])(void *);   // Functions call in the ISR context!!

static void prvAppTimersTask( void *pvParameters );
static void vAppTimerInit(pAppTimerCntl master_pAppTimerMsg);
static void apptimer_isr(void *arg);

/*-----------------------------------------------------------*/

void vAppTimersStart( uint16_t usStackSize, portBASE_TYPE uxPriority )
{
	/* Create the queues used by application  tasks and interrupts */
	xAppTimersInQueue = xQueueCreate( AppTimersQUEUE_LENGTH, sizeof(AppTimerCntl_t) );
	xAppTimersOutQueue = xQueueCreate( 1, sizeof(AppTimerCntl_t) );
	xAppTimer00OutQueue = xQueueCreate( 1, sizeof(AppTimerCntl_t) );
	xAppTimer01OutQueue = xQueueCreate( 1, sizeof(AppTimerCntl_t) );
	xAppTimer10OutQueue = xQueueCreate( 1, sizeof(AppTimerCntl_t) );
	xAppTimer11OutQueue = xQueueCreate( 1, sizeof(AppTimerCntl_t) );

	/* If the queue could not be created then don't create any tasks that might
	attempt to use the queue. */
	if( xAppTimersInQueue != NULL && xAppTimersOutQueue != NULL
		&& xAppTimer00OutQueue != NULL 	
		&& xAppTimer01OutQueue != NULL 	
		&& xAppTimer10OutQueue != NULL 	
		&& xAppTimer11OutQueue != NULL 	
	)
	{
	
		/* Create the semaphore used to access the Timer(s) transactions. */
		xAppTimersMutex = xSemaphoreCreateMutex();
		configASSERT( xAppTimersMutex );

		/* Create that task that handles the Timers*/
		xTaskCreate(prvAppTimersTask,					/* The task that implements the application. */
					(const char *const) "Timers",		/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
					usStackSize,					/* The size of the stack allocated to the task. */
					NULL,						/* The parameter is not used, so NULL is passed. */
					uxPriority,					/* The priority allocated to the task. */
					NULL );						/* A handle is not required, so just pass NULL. */

	}
}

/*-----------------------------------------------------------*/

static void prvAppTimersTask( void *pvParameters )
{
	AppTimerCntl_t _AppTimerMsg;   // current transaction ..
	( void ) pvParameters;

		master_pAppTimerMsg = &_AppTimerMsg;

		while(1)
		{	
			if( xQueueReceive( xAppTimersInQueue, master_pAppTimerMsg, portMAX_DELAY) == pdPASS)
			{
				if (master_pAppTimerMsg->app_msg.cmd) {
					switch (master_pAppTimerMsg->app_msg.cmd)
					{
					case APPMSG_GET_STACK_ROOM:
							master_pAppTimerMsg->app_msg.d.v= uxTaskGetStackHighWaterMark(NULL);
						break;
					case APPMSG_TIMER_INIT:
							vAppTimerInit(master_pAppTimerMsg);
						break;
					case APPMSG_TIMER_START:
							if (master_pAppTimerMsg->timer_idx == 0)	{
								timer_enable_intr(TIMER_GROUP_0, TIMER_0);
								timer_start(TIMER_GROUP_0, TIMER_0);
							}
							else
							if (master_pAppTimerMsg->timer_idx == 1)	{
								timer_enable_intr(TIMER_GROUP_0, TIMER_1);
								timer_start(TIMER_GROUP_0, TIMER_1);
							}
							else
							if (master_pAppTimerMsg->timer_idx == 2)	{
								timer_enable_intr(TIMER_GROUP_1, TIMER_0);
								timer_start(TIMER_GROUP_1, TIMER_0);
							}
							else
							if (master_pAppTimerMsg->timer_idx == 3)	{
								timer_enable_intr(TIMER_GROUP_1, TIMER_1);
								timer_start(TIMER_GROUP_1, TIMER_1);
							}
						break;
					case APPMSG_TIMER_STOP:
							if (master_pAppTimerMsg->timer_idx == 0)	{
								timer_pause(TIMER_GROUP_0, TIMER_0);
								timer_disable_intr(TIMER_GROUP_0, TIMER_0);
								timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);
							}
							else
							if (master_pAppTimerMsg->timer_idx == 1)	{
								timer_pause(TIMER_GROUP_0, TIMER_1);
								timer_disable_intr(TIMER_GROUP_0, TIMER_1);
								timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 0x00000000ULL);
							}
							else
							if (master_pAppTimerMsg->timer_idx == 2)	{
								timer_pause(TIMER_GROUP_1, TIMER_0);
								timer_disable_intr(TIMER_GROUP_1, TIMER_0);
								timer_set_counter_value(TIMER_GROUP_1, TIMER_0, 0x00000000ULL);
							}
							else
							if (master_pAppTimerMsg->timer_idx == 3)	{
								timer_pause(TIMER_GROUP_1, TIMER_1);
								timer_disable_intr(TIMER_GROUP_1, TIMER_1);
								timer_set_counter_value(TIMER_GROUP_1, TIMER_1, 0x00000000ULL);
							}
						break;
					}
					if (master_pAppTimerMsg->app_msg.app__doneCallback != NULL)
						master_pAppTimerMsg->app_msg.app__doneCallback(master_pAppTimerMsg);

				}
			}
		}
}



/*-----------------------------------------------------------*/
static void vAppTimerInit(pAppTimerCntl pAppTimerMsg)
{
	int timer_group =0, timer_id= 0;
    	timer_config_t config;
	uint64_t  val = 0x00000000ULL;


	switch(pAppTimerMsg->timer_idx) 
	{
		case 0: timer_group = TIMER_GROUP_0; timer_id = TIMER_0; break;
		case 1: timer_group = TIMER_GROUP_0; timer_id = TIMER_1; break;
		case 2: timer_group = TIMER_GROUP_1; timer_id = TIMER_0; break;
		case 3: timer_group = TIMER_GROUP_1; timer_id = TIMER_1; break;
		default:
			VApplicationGeneralFault;
	}

// printf("%s: load #%d with alarn=%d\r\n", __func__, pAppTimerMsg->timer_idx, pAppTimerMsg->app_msg.d.v);

    config.alarm_en = 1;
    config.auto_reload = 1;
    config.counter_dir = TIMER_COUNT_UP;
    config.divider = TIMER_DIVIDER;
    config.intr_type = TIMER_INTR_LEVEL;
    config.counter_en = TIMER_PAUSE;
    /*Configure timer*/
    ESP_ERROR_CHECK( timer_init(timer_group, timer_id, &config));
    /*Stop timer counter*/
    timer_pause(timer_group, timer_id);
    /*Load counter value */
    timer_set_counter_value(timer_group, timer_id, 0x00000000ULL);
    /*Set alarm value*/
    val = 	pAppTimerMsg->app_msg.d.v;
    timer_set_alarm_value(timer_group, timer_id, val);
    /*Enable timer interrupt at start ..*/
//    timer_enable_intr(timer_group, timer_id);
    timer_disable_intr(timer_group, timer_id);

    ESP_ERROR_CHECK( timer_isr_register(timer_group, timer_id, apptimer_isr, 
		(void*)&inth[ pAppTimerMsg->timer_idx], 0, &inth[ pAppTimerMsg->timer_idx]));

    fn__Callback[pAppTimerMsg->timer_idx] = pAppTimerMsg->apptimer__doneCallback;
	
}

static void apptimer_isr(void *arg)
{
    int *timer_idx = (int *)arg;
    AppTimerCntl_t	ISRMsg;

    ISRMsg.app_msg.cmd = APPMSG_TIMER_INTR;
//    count[timer_idx]++;
    if (*timer_idx== (int)inth[ 0]) {
        TIMERG0.int_clr_timers.t0 = 1;
        TIMERG0.hw_timer[0].update=1;
        TIMERG0.hw_timer[0].config.alarm_en = 1;
    	  ISRMsg.timer_idx = 0;
	  if (fn__Callback[ 0] != NULL)	
	  	fn__Callback[ 0](&ISRMsg);
	  else
    	  	xQueueSendFromISR(xAppTimer00OutQueue, &ISRMsg,  NULL );
    }else
    if (*timer_idx== (int)inth[ 1]) {
        TIMERG0.int_clr_timers.t1 = 1;
        TIMERG0.hw_timer[1].update=1;
        TIMERG0.hw_timer[1].config.alarm_en = 1;
    	  ISRMsg.timer_idx = 1;
	  if (fn__Callback[ 1] != NULL)	
	  	fn__Callback[ 1](&ISRMsg);
	  else
    	  	xQueueSendFromISR(xAppTimer01OutQueue, &ISRMsg,  NULL );
    }else
    if (*timer_idx== (int)inth[ 2]) {
        TIMERG1.int_clr_timers.t0 = 1;
        TIMERG1.hw_timer[0].update=1;
        TIMERG1.hw_timer[0].config.alarm_en = 1;
    	  ISRMsg.timer_idx = 2;
	  if (fn__Callback[ 2] != NULL)	
	  	fn__Callback[ 2](&ISRMsg);
	  else
    	  	xQueueSendFromISR(xAppTimer10OutQueue, &ISRMsg,  NULL );
    }else
    if (*timer_idx== (int)inth[ 3]) {
        TIMERG1.int_clr_timers.t1 = 1;
        TIMERG1.hw_timer[1].update=1;
        TIMERG1.hw_timer[1].config.alarm_en = 1;
    	  ISRMsg.timer_idx = 3;
	  if (fn__Callback[ 3] != NULL)	
	  	fn__Callback[ 3](&ISRMsg);
	  else
    	  	xQueueSendFromISR(xAppTimer11OutQueue, &ISRMsg,  NULL );
    }
//  ets_printf("int %d\n", timer_idx);
}

/*-----------------------------------------------------------*/

portBASE_TYPE AppTimerMsgPut(pAppTimerCntl msg)
{
	if(xQueueSend(xAppTimersInQueue, msg, ( portTickType ) 10) != pdPASS)
		{
			VApplicationGeneralFault;
		}
	return pdPASS;
}

portBASE_TYPE  AppTimerOnDone(void *pvParameters)
{
	pAppTimerCntl msg = (pAppTimerCntl)pvParameters;

	if(xQueueSend(xAppTimersOutQueue, msg, ( portTickType ) (10 /portTICK_RATE_MS)) != pdPASS)
	{
		VApplicationGeneralFault;
	}
		
	return pdPASS;
}	

portBASE_TYPE AppTimerMsgGet(pAppTimerCntl msg)
{
	return xQueueReceive(xAppTimersOutQueue, msg, portMAX_DELAY);
}

/*-----------------------------------------------------------*/
//   Default action. Call in the ISR context. It equal to what ISR did in the case 
//   when the pointer to it is NULL.  User defined routine can call it as a final step.
//   The data union in the app_msg not used, and we can use it for cancel a DMA/WiFi 
//   transaction in case a timeout.

portBASE_TYPE  AppTimer0OnDone(void *pvParameters)
{
	pAppTimerCntl msg = (pAppTimerCntl)pvParameters;

    	xQueueSendFromISR(xAppTimer00OutQueue, msg,  NULL );
		
	return pdPASS;
}	

portBASE_TYPE AppTimer0MsgGet(pAppTimerCntl msg)
{
	return xQueueReceive(xAppTimer00OutQueue, msg, portMAX_DELAY);
}

portBASE_TYPE AppTimer0MsgGetTimeout(pAppTimerCntl msg, TickType_t delay)
{
	return xQueueReceive(xAppTimer00OutQueue, msg, delay);
}

portBASE_TYPE  AppTimer1OnDone(void *pvParameters)
{
	pAppTimerCntl msg = (pAppTimerCntl)pvParameters;

    	xQueueSendFromISR(xAppTimer01OutQueue, msg,  NULL );
		
	return pdPASS;
}	

portBASE_TYPE AppTimer1MsgGet(pAppTimerCntl msg)
{
	return xQueueReceive(xAppTimer01OutQueue, msg, portMAX_DELAY);
}

portBASE_TYPE  AppTimer2OnDone(void *pvParameters)
{
	pAppTimerCntl msg = (pAppTimerCntl)pvParameters;

    	xQueueSendFromISR(xAppTimer10OutQueue, msg,  NULL );
		
	return pdPASS;
}	

portBASE_TYPE AppTimer2MsgGet(pAppTimerCntl msg)
{
	return xQueueReceive(xAppTimer10OutQueue, msg, portMAX_DELAY);
}

portBASE_TYPE  AppTimer3OnDone(void *pvParameters)
{
	pAppTimerCntl msg = (pAppTimerCntl)pvParameters;

    	xQueueSendFromISR(xAppTimer11OutQueue, msg,  NULL );
		
	return pdPASS;
}	

portBASE_TYPE AppTimer3MgGet(pAppTimerCntl msg)
{
	return xQueueReceive(xAppTimer11OutQueue, msg, portMAX_DELAY);
}

portBASE_TYPE AppTimersTake(void *pvParameters)
{
	( void ) pvParameters;
	if (xAppTimersMutex == NULL)  return pdFAIL;

        // Obtain the semaphore - don't block even if the semaphore is not
        // available during a time more then a reasonable.
	if( xSemaphoreTake( xAppTimersMutex, (portMAX_DELAY -10) )  != pdPASS )
	{
		// Never return from  ..
		{
						VApplicationGeneralFault;
		}
	}
	return pdPASS;
}
portBASE_TYPE AppTimersGive(void *pvParameters)
{
	( void ) pvParameters;
	if (xAppTimersMutex == NULL)  return pdFAIL;

	if( xSemaphoreGive( xAppTimersMutex ) != pdPASS )
	{
		// Never return from  ..
		{
						VApplicationGeneralFault;
		}
	}
	return pdPASS;
}

/*-----------------------------------------------------------*/

// Return the value for set to a timer in timer's clocks units for a number of secs 

int getAppTimerVal4Secs(int secs) 
{
	return (int)(secs * TIMER_SCALE); 
}

void getAppTimerVal(int timer_idx, uint64_t* timer_val)
{
	int timer_group =0, timer_id= 0;

	switch(timer_idx) 
	{
		case 0: timer_group = TIMER_GROUP_0; timer_id = TIMER_0; break;
		case 1: timer_group = TIMER_GROUP_0; timer_id = TIMER_1; break;
		case 2: timer_group = TIMER_GROUP_1; timer_id = TIMER_0; break;
		case 3: timer_group = TIMER_GROUP_1; timer_id = TIMER_1; break;
		default:
			VApplicationGeneralFault;
	}

	timer_get_counter_value(timer_group, timer_id, timer_val);
}
