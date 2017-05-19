/*
 * CLI_commands.c
 *
 *  Created on: May 3, 2017
 *      Author: beam
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

/* Application related includes */
#include "app.h"
#include "board.h"

/*-----------------------------------------------------------*/
static portBASE_TYPE esp32_resurces_command(int8_t *pcWriteBuffer, 	size_t xWriteBufferLen, const int8_t *pcCommandString);
static portBASE_TYPE esp32_i2c_command(int8_t *pcWriteBuffer, 	size_t xWriteBufferLen, const int8_t *pcCommandString);
#ifdef CONFIG_SSD1306_OLED
static portBASE_TYPE oled_output_command(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);
#endif // CONFIG_SSD1306_OLED


static const CLI_Command_Definition_t esp32_resurces_command_definition =
{
	(const int8_t *const) "res", /* The command string to type. */
	(const int8_t *const) "res\tShow resources\r\n",
	esp32_resurces_command, /* The function to run. */
	0 /* No parameters are expected. */
};

static const CLI_Command_Definition_t esp32_i2c_command_definition =
{
	(const int8_t *const) "i2c", /* The command string to type. */
	(const int8_t *const) "i2c\tr|w <addr> <value>\tRead/Write EEPROM value from/to address\r\n",
	esp32_i2c_command, /* The function to run. */
	3 /* Three parameters are expected. */
};

#ifdef CONFIG_SSD1306_OLED
static const CLI_Command_Definition_t oled_output_command_definition =
{
	(const int8_t *const) "oled", /* The command string to type. */
	(const int8_t *const) "oled\t<line> <column> <string>  where: line {[0-3] |clean}, colum [0-128]\r\n",
	oled_output_command, /* The function to run. */
	3 /* Three parameters are expected. */
};
#endif // CONFIG_SSD1306_OLED

/*-----------------------------------------------------------*/

void vRegisterCLICommands(void)
{
	/* Register all the command line commands defined immediately above. */
	FreeRTOS_CLIRegisterCommand(&esp32_resurces_command_definition);
	FreeRTOS_CLIRegisterCommand(&esp32_i2c_command_definition);
#ifdef CONFIG_SSD1306_OLED
	FreeRTOS_CLIRegisterCommand(&oled_output_command_definition);
#endif // CONFIG_SSD1306_OLED	
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

	printf("Tasks: %d\r\n", uxTaskGetNumberOfTasks());
	printf("RAM left %d\r\n", esp_get_free_heap_size());
	printf("CLI task stack: %d", uxTaskGetStackHighWaterMark(NULL));
//	printf("CLI task priority: %d", uxTaskPriorityGet(NULL));

	*pcWriteBuffer = 0x00;
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

static portBASE_TYPE esp32_i2c_command(int8_t *pcWriteBuffer, 	size_t xWriteBufferLen, const int8_t *pcCommandString)
{
	int8_t *parameter_string;
	portBASE_TYPE parameter_string_length;
	static uint8_t  addr[2], value;
	unsigned long tmp;
	I2CMessage_t msg;

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	(void) xWriteBufferLen;
	configASSERT(pcWriteBuffer);

	/* Obtain the 1st parameter. */
	parameter_string = (int8_t *) FreeRTOS_CLIGetParameter(
									pcCommandString,		/* The command string itself. */
									1,						/* Return the first parameter. */
									&parameter_string_length	/* Store the parameter string length. */
								);

	/* Obtain the 2nd parameter. */
	
	if (*parameter_string == 'r') {
		
		parameter_string = (int8_t *) FreeRTOS_CLIGetParameter(
								pcCommandString,		/* The command string itself. */
								2,						/* Return the second parameter. */
								&parameter_string_length	/* Store the parameter string length. */
							);
		
		tmp = strtoul((const char *)parameter_string, NULL, 16);
		addr[0] = tmp&0xff;
		
		msg.tx.length = 1;
		msg.tx.chip = AT30TSE758_EEPROM_ADDR;
		msg.tx.buffer = &addr[0];
		msg.rx.length = 1;
		msg.rx.chip = AT30TSE758_EEPROM_ADDR;
		msg.rx.buffer = &value;
		msg.i2c__doneCallback = I2C0OnDone;
		I2C0Take(NULL);
		I2C0MsgPut(&msg);
		while (I2C0MsgGet(&msg) != pdFAIL) {
			break;
	 	}
		I2C0Give(NULL);

		printf("value 0x%02x\r\n", value);
		
	}else
	if (*parameter_string == 'w') {
		
		parameter_string = (int8_t *) FreeRTOS_CLIGetParameter(
								pcCommandString,		/* The command string itself. */
								2,						/* Return the second parameter. */
								&parameter_string_length	/* Store the parameter string length. */
							);
		
		tmp = strtoul((const char *)parameter_string, NULL, 16);
		addr[0] = tmp&0xff;
		
		/* Obtain the 3rd parameter. */
		parameter_string = (int8_t *) FreeRTOS_CLIGetParameter(
								pcCommandString,		/* The command string itself. */
								3,						/* Return the second parameter. */
								&parameter_string_length	/* Store the parameter string length. */
								);
		
		tmp = strtoul((const char *)parameter_string, NULL, 16);
		addr[1] = tmp&0xff;

		msg.tx.length = 2;
		msg.tx.chip = AT30TSE758_EEPROM_ADDR;
		msg.tx.buffer = &addr;
		msg.rx.length = 0;
		msg.rx.chip = AT30TSE758_EEPROM_ADDR;
		msg.rx.buffer = NULL;
		msg.i2c__doneCallback = I2C0OnDone;
		I2C0Take(NULL);
		I2C0MsgPut(&msg);
		while (I2C0MsgGet(&msg) != pdFAIL) {
			break;
	 	}
		I2C0Give(NULL);

	}

	*pcWriteBuffer = 0x00;
	return pdFALSE;
}

#ifdef CONFIG_SSD1306_OLED
/*-----------------------------------------------------------*/
static portBASE_TYPE oled_output_command(int8_t *pcWriteBuffer,
		size_t xWriteBufferLen,
		const int8_t *pcCommandString)
{

	int8_t *parameter1_string, *parameter2_string, *parameter3_string;
	portBASE_TYPE parameter_string_length;
	static const int8_t *failure_message = (int8_t *) "*** ERROR: Uncorrect parameter\r\n";
	uint8_t page, column; 
	OledMessage_t msg;

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	(void) xWriteBufferLen;
	configASSERT(pcWriteBuffer);

	/* Obtain the parameter string. */
	parameter1_string = (int8_t *) FreeRTOS_CLIGetParameter(
									pcCommandString,		/* The command string itself. */
									1,						/* Return the first parameter. */
									&parameter_string_length	/* Store the parameter string length. */
								);
	*pcWriteBuffer = '\0';
	if (!strncmp((const char *)parameter1_string, "clean", 5) )
	{
		uint8_t  clbuff[129];

		memset(clbuff, 0x00, 128); clbuff[128]=0x00;
		for (page= 0; page < 4; page++) {
		
//		msg.app_msg.cmd = APPMSG_OLED_CLEAN;
			msg.app_msg.cmd = APPMSG_OLED_CLEAN;
			msg.y = page;
			msg.x = 0;
			msg.str = (unsigned char *)clbuff;
			msg.strLen = 128;
			msg.oled__doneCallback = OledOnDone;
			OledMsgPut(&msg);
			while(OledMsgGet(&msg) != pdFAIL) {
				break;
			}
		}
		goto finish;
	}else{
		page = atoi((char *)parameter1_string);
		if (/*page != 0 && */ page < 4) {
			parameter2_string = (int8_t *) FreeRTOS_CLIGetParameter(
									pcCommandString,		/* The command string itself. */
									2,						/* Return the first parameter. */
									&parameter_string_length	/* Store the parameter string length. */
								);
			if (parameter_string_length) {
				column = atoi((char *)parameter2_string);
				if (/* column !=0 && */ column <128) {					

					parameter3_string = (int8_t *) FreeRTOS_CLIGetParameter(
									pcCommandString,		/* The command string itself. */
									3,						/* Return the first parameter. */
									&parameter_string_length	/* Store the parameter string length. */
								);
					if (parameter_string_length) {
						msg.app_msg.cmd = APPMSG_OLED_SHOW;
						msg.y = page;
						msg.x = column;
						msg.str = (unsigned char *)parameter3_string;
						msg.strLen = parameter_string_length;
						msg.oled__doneCallback = OledOnDone;
						OledMsgPut(&msg);

						while(OledMsgGet(&msg) != pdFAIL) {
							break;
						}
						goto finish;
					}
				}
			}
		}
	}
	strcpy((char * restrict)pcWriteBuffer, (const char *) failure_message);
finish:
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}
/*-----------------------------------------------------------*/

#endif // CONFIG_SSD1306_OLED

