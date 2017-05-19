/*
 * task_i2c.c
 *
 *  Created on: May 11, 2017
 *      Author: beam
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/i2c.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "FreeRTOS_CLI.h"
#include "app.h"


static const char *TAG = "I2C_master";

#define I2C_MASTER_SCL_IO    22    /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO    21    /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */

#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */

#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
/** Delay between two continue I2C read or write operation in microsecond */
#define I2C0_CONTINUE_RW_DELAY  50



#define i2cQUEUE_LENGTH				( 2 )

static xQueueHandle xI2C0MasterInQueue = NULL;
static xQueueHandle xI2C0MasterOutQueue = NULL;
static xSemaphoreHandle xI2C0Mutex = NULL;

static void prvI2C0MasterTask( void *pvParameters );

static pI2CMessage master_pI2CMsg = NULL;   // current transaction ..
static unsigned char I2C0_init = 0;


/**
 * @brief i2c master initialization
 */
static void i2c_master_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * @brief code to read esp-i2c
 *        We need to fill the buffer of esp slave device, then master can read them out.
 *
 * _______________________________________________________________________________________
 * | start | slave_addr + rd_bit +ack | read n-1 bytes + ack | read 1 byte + nack | stop |
 * |-----|----------------------|-------------------|-----------------|----|
 *
 */
static esp_err_t i2c_master_read_slave(i2c_port_t i2c_num, uint8_t addr,  uint8_t* data_rd, size_t size)
{
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( addr << 1 ) | READ_BIT, ACK_CHECK_EN);
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief  code to write esp-i2c
 *        Master device write data to slave.
 *
 * ___________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write n bytes + ack  | stop |
 * |-----|-----------------------|------------------|-----|
 *
 */
static esp_err_t i2c_master_write_slave(i2c_port_t i2c_num, uint8_t addr, uint8_t* data_wr, size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( addr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/*-----------------------------------------------------------*/

void vI2C0MasterStart( uint16_t usStackSize, UBaseType_t uxPriority)
{
	i2c_master_init();

	/* Create the queues used by tasks and interrupts to send/receive SPI data. */
	xI2C0MasterInQueue = xQueueCreate( i2cQUEUE_LENGTH, sizeof(I2CMessage_t) );
	xI2C0MasterOutQueue = xQueueCreate( 1, sizeof(I2CMessage_t) );

	/* If the queue could not be created then don't create any tasks that might
	attempt to use the queue. */
	if( xI2C0MasterInQueue != NULL && xI2C0MasterOutQueue != NULL)
	{
	
		/* Create the semaphore used to access the I2C transmission. */
		xI2C0Mutex = xSemaphoreCreateMutex();
		configASSERT( xI2C0Mutex );
		/* Create that task that handles the I2C Master itself. */
		xTaskCreate(	prvI2C0MasterTask,			/* The task that implements the command console. */
						(const char *const) "I2C0",		/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
						usStackSize,					/* The size of the stack allocated to the task. */
						NULL,						/* The parameter is not used, so NULL is passed. */
						uxPriority,					/* The priority allocated to the task. */
						NULL );						/* A handle is not required, so just pass NULL. */
	}
}


/*-----------------------------------------------------------*/
static void prvI2C0MasterTask( void *pvParameters )
{
	I2CMessage_t _I2CMsg;   // current transaction ..
	( void ) pvParameters;
	int ret;

		master_pI2CMsg = &_I2CMsg;
		while(1)
		{
			I2C0_init = 1;
			if( xQueueReceive( xI2C0MasterInQueue, master_pI2CMsg, portMAX_DELAY) == pdPASS)
			{
				if (master_pI2CMsg->tx.length != 0) {
					ret =  i2c_master_write_slave(	I2C_MASTER_NUM,						/* Controller */ 
											master_pI2CMsg->tx.chip,					/* Slave address */
											(uint8_t *)master_pI2CMsg->tx.buffer,		/* data buffer */
											master_pI2CMsg->tx.length				/* data length */
											);
					if (ret != ESP_OK) {
printf("ret = 0x%x\r\n", ret);					
						VApplicationGeneralFault;
					}
					
					if (master_pI2CMsg->rx.length != 0) {
						/*
						 * Delay between each write or read cycle between TWI Stop and TWI Start.
						 */
						 vTaskDelay(I2C0_CONTINUE_RW_DELAY/portTICK_RATE_MS);

						ret = i2c_master_read_slave(I2C_MASTER_NUM,						/* Controller */ 
													master_pI2CMsg->rx.chip,				/* Slave address */		
													(uint8_t *)master_pI2CMsg->rx.buffer,	/* data buffer */
													master_pI2CMsg->rx.length			/* data length */
													);
						if (ret != ESP_OK)
							VApplicationGeneralFault;

					}
				}else
					if (master_pI2CMsg->rx.length != 0) {
						ret = i2c_master_read_slave(I2C_MASTER_NUM,						/* Controller */ 
													master_pI2CMsg->rx.chip,				/* Slave address */		
													(uint8_t *)master_pI2CMsg->rx.buffer,	/* data buffer */
													master_pI2CMsg->rx.length			/* data length */
													);
					if (ret != ESP_OK)
						VApplicationGeneralFault;
			}
			if (master_pI2CMsg->i2c__doneCallback != NULL)
				master_pI2CMsg->i2c__doneCallback(master_pI2CMsg);
			}
		}
}

/*-----------------------------------------------------------*/
portBASE_TYPE isI2C0Ready(void)
{
	if (I2C0_init) 	return pdTRUE;
	return pdFALSE;
}
portBASE_TYPE I2C0MsgPut(pI2CMessage msg)
{

	if(xQueueSend(xI2C0MasterInQueue, msg, ( portTickType ) 10) != pdPASS)
		{
						VApplicationGeneralFault;
		}
	return pdPASS;
}

portBASE_TYPE I2C0MsgGet(pI2CMessage msg)
{
	return xQueueReceive(xI2C0MasterOutQueue, msg, portMAX_DELAY);
}

portBASE_TYPE  I2C0OnDone(void *pvParameters)
{
	pI2CMessage msg = (pI2CMessage)pvParameters;

	if(xQueueSend(xI2C0MasterOutQueue, msg, ( portTickType ) (10 /portTICK_RATE_MS)) != pdPASS)
	{
						VApplicationGeneralFault;
	}
		
	return pdPASS;
}	
portBASE_TYPE I2C0Take(void *pvParameters)
{
	( void ) pvParameters;
	if (xI2C0Mutex == NULL)  return pdFAIL;

        // Obtain the semaphore - don't block even if the semaphore is not
        // available during a time more then a reasonable.
	if( xSemaphoreTake( xI2C0Mutex, (portMAX_DELAY -10) )  != pdPASS )
	{
		// Never return from  ..
		{
						VApplicationGeneralFault;
		}
	}
	return pdPASS;
}
portBASE_TYPE I2C0Give(void *pvParameters)
{
	( void ) pvParameters;
	if (xI2C0Mutex == NULL)  return pdFAIL;

	if( xSemaphoreGive( xI2C0Mutex ) != pdPASS )
	{
		// Never return from  ..
		{
						VApplicationGeneralFault;
		}
	}
	return pdPASS;
}
/*-----------------------------------------------------------*/


