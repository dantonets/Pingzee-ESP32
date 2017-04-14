/*
 * CLI_commands.c
 *
 * Created: 8/4/2016 4:15:42 PM
 *  Author: jbahr
 */ 
/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
// ESP headers
#include "esp_system.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

/*-----------------------------------------------------------*/
static portBASE_TYPE esp32_resurces_command(int8_t *pcWriteBuffer, 	size_t xWriteBufferLen, const int8_t *pcCommandString);


static const CLI_Command_Definition_t esp32_resurces_command_definition =
{
	(const int8_t *const) "res", /* The command string to type. */
	(const int8_t *const) "res\tShow resources\r\n",
	esp32_resurces_command, /* The function to run. */
	0 /* No parameters are expected. */
};

/*-----------------------------------------------------------*/

void vRegisterCLICommands(void)
{
	/* Register all the command line commands defined immediately above. */
	FreeRTOS_CLIRegisterCommand(&esp32_resurces_command_definition);
	
}

static portBASE_TYPE esp32_resurces_command(int8_t *pcWriteBuffer, 	size_t xWriteBufferLen, const int8_t *pcCommandString)
{
//	int8_t *parameter1_string;
//	portBASE_TYPE parameter_string_length;

	
	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	(void) xWriteBufferLen;
	configASSERT(pcWriteBuffer);
	
	printf("RAM left %d\r\n", esp_get_free_heap_size());
//	printf("CLI task stack: %d", uxTaskGetStackHighWaterMark(NULL));
	
	*pcWriteBuffer = 0x00;
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}


