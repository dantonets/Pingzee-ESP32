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


static const char *APP_TAG = "App";



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

