/*
 * uart_console.c
 *
 *  Created on: May 3, 2017
 *      Author: beam
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "soc/uart_struct.h"
#include "FreeRTOS_CLI.h"

static const char *TAG = "uart_console";

#define BUF_SIZE (65)
#define ECHO_TEST_TXD  (4)
#define ECHO_TEST_RXD  (5)
#define ECHO_TEST_RTS  (18)
#define ECHO_TEST_CTS  (19)
/* Dimensions the buffer into which input characters are placed. */
#define MAX_INPUT_SIZE          50

void vRegisterCLICommands(void);

static QueueHandle_t uart0_queue;

static const uint8_t *const welcome_message = (uint8_t *) "> ";
static const uint8_t *const new_line = (uint8_t *) "\r\n";
static const uint8_t *const line_separator = (uint8_t *) "\r\n> ";
static int8_t input_string[MAX_INPUT_SIZE],
			last_input_string[MAX_INPUT_SIZE];

#if 0
static void uart_task(void *pvParameters)
{
    int uart_num = (int) pvParameters;
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(BUF_SIZE);
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(uart0_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
//            ESP_LOGI(TAG, "uart[%d] event:", uart_num);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.
                in this example, we don't process data in event, but read data outside.*/
                case UART_DATA:
                    uart_get_buffered_data_len(uart_num, &buffered_size);
//                    ESP_LOGI(TAG, "data, len: %d; buffered len: %d", event.size, buffered_size);
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow\n");
                    //If fifo overflow happened, you should consider adding flow control for your application.
                    //We can read data out out the buffer, or directly flush the rx buffer.
                    uart_flush(uart_num);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full\n");
                    //If buffer full happened, you should consider encreasing your buffer size
                    //We can read data out out the buffer, or directly flush the rx buffer.
                    uart_flush(uart_num);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break\n");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error\n");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error\n");
                    break;
                //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    ESP_LOGI(TAG, "uart pattern detected\n");
                    break;
                //Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d\n", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}
#endif // 0
static void cli_task(void *pvParameters)
{
    int uart_num = (int) pvParameters;
    int len;
    //process data
//    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    uint8_t data[3];
    uint8_t *received_char, input_index = 0, *output_string;
    portBASE_TYPE returned_value;
    portTickType max_block_time_ticks = 20UL / portTICK_RATE_MS;
	
ESP_LOGI(TAG, "uart[%d] cli_task started", uart_num);
	
    output_string = (uint8_t *) FreeRTOS_CLIGetOutputBuffer();
    strcpy((char *) output_string, (char *) welcome_message);
    printf("%s", (const char *)output_string); fflush(stdout);
    received_char = &data[0];
	
    do {
//        len = uart_read_bytes(uart_num, data, BUF_SIZE, 100 / portTICK_RATE_MS);
        len = uart_read_bytes(uart_num, data, 1, 10 / portTICK_RATE_MS);
        if(len > 0) {
//            ESP_LOGI(TAG, "uart read : %d", len);
// Echo input 
//        uart_write_bytes(uart_num, (const char*)data, len);
// [ADK] Disabled, use the "Local echo" on the TeraTerm
		putchar(data[0]);	fflush(stdout);
		if (*received_char == '\r') {
			/* Start to transmit a line separator, just to make the output 	easier to read. */
			strcpy((char *)output_string, (char *) new_line);
			printf("%s", (const char *)output_string);

			/* See if the command is empty, indicating that the last command 	is to be executed again. */
			if (input_index == 0) {
				strcpy((char *) input_string, 	(char *) last_input_string);
			}

			/* Pass the received command to the command interpreter.  The
			  command interpreter is called repeatedly until it returns pdFALSE as
			  it might generate more than one string. 
			*/
			do {
				/* Get the string to write to the UART from the command
					interpreter. */
					returned_value = FreeRTOS_CLIProcessCommand(
							input_string,
							(int8_t *) output_string,
							configCOMMAND_INT_MAX_OUTPUT_SIZE);

					/* Start the USART transmitting the generated string. */
					printf("%s", (const char *)output_string);
				} while (returned_value != pdFALSE);

				/* All the strings generated by the input command have been sent.
				Clear the input	string ready to receive the next command.
				Remember the command that was just processed first in case it is
				to be processed again. */
				strcpy((char *) last_input_string, 	(char *) input_string);
				input_index = 0;
				memset(input_string, 0x00, MAX_INPUT_SIZE);

				/* Start to transmit a line separator, just to make the output
				easier to read. */
				strcpy((char *) output_string, (char *) line_separator);
				printf("%s", (const char *)output_string); fflush(stdout);
			} else {
				if (*received_char == '\n') {
					/* Ignore the character. */
				} else if (*received_char == '\b') {
					/* Backspace was pressed.  Erase the last character in the
					string - if any. */
					printf(" \b");
					if (input_index > 0) {
						input_index--;
						input_string[input_index] = '\0';
					}
				} else {
					/* A character was entered.  Add it to the string
					entered so far.  When a \n is entered the complete
					string will be passed to the command interpreter. */
					if (input_index < MAX_INPUT_SIZE) {
						input_string[input_index] = *received_char;
						input_index++;
					}
				}
			}
		}
/* == we have delay in the uart_read_bytes()
		else {
			vTaskDelay(max_block_time_ticks);
		}
*/		
    } while(1);
//    free(data);
//    data = NULL;
    vTaskDelete(NULL);
}

void uart_console_task(void)
{
    int uart_num = UART_NUM_0;
    uart_config_t uart_config = {
       .baud_rate = 115200,
       .data_bits = UART_DATA_8_BITS,
       .parity = UART_PARITY_DISABLE,
       .stop_bits = UART_STOP_BITS_1,
       .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
       .rx_flow_ctrl_thresh = 122,
    };
 ESP_LOGI(TAG, "uart[%d]: %s", uart_num, __func__);
   //Set UART parameters
    uart_param_config(uart_num, &uart_config);
    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Install UART driver, and get the queue.
//    uart_driver_install(uart_num, BUF_SIZE * 2, BUF_SIZE * 2, 10, &uart0_queue, 0);
    uart_driver_install(uart_num, BUF_SIZE * 2, BUF_SIZE * 2, 10, NULL, 0);
    //Set UART pins,(-1: default pin, no change.)
    //For UART0, we can just use the default pins.
    //uart_set_pin(uart_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //Set uart pattern detect function.
    uart_enable_pattern_det_intr(uart_num, '+', 3, 10000, 10, 10);
    //Create a task to handler UART event from ISR

    vRegisterCLICommands();	
	
//    xTaskCreate(uart_task, "uart_task", 2048, (void*)uart_num, 12, NULL);
    xTaskCreate(cli_task, "cli_task", 2048, (void*)uart_num, 12, NULL);
}


