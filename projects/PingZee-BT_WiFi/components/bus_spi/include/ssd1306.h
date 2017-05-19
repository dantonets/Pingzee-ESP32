/*
 * ssd1306.h
 *
 *
 * \brief SSD1306 OLED display controller driver.
 * 
 *  Created on: May 16, 2017
 *      Author: beam
 */

#ifndef COMPONENTS_BUS_SPI_INCLUDE_SSD1306_H_
#define COMPONENTS_BUS_SPI_INCLUDE_SSD1306_H_

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
#include "board.h"

//! \name Fundamental Command defines
//@{
#define SSD1306_CMD_SET_LOW_COL(column)             (0x00 | (column))
#define SSD1306_CMD_SET_HIGH_COL(column)            (0x10 | (column))
#define SSD1306_CMD_SET_MEMORY_ADDRESSING_MODE      0x20
#define SSD1306_CMD_SET_COLUMN_ADDRESS              0x21
#define SSD1306_CMD_SET_PAGE_ADDRESS                0x22
#define SSD1306_CMD_SET_START_LINE(line)            (0x40 | (line))
#define SSD1306_CMD_SET_CONTRAST_CONTROL_FOR_BANK0  0x81
#define SSD1306_CMD_SET_CHARGE_PUMP_SETTING         0x8D
#define SSD1306_CMD_SET_SEGMENT_RE_MAP_COL0_SEG0    0xA0
#define SSD1306_CMD_SET_SEGMENT_RE_MAP_COL127_SEG0  0xA1
#define SSD1306_CMD_ENTIRE_DISPLAY_AND_GDDRAM_ON    0xA4
#define SSD1306_CMD_ENTIRE_DISPLAY_ON               0xA5
#define SSD1306_CMD_SET_NORMAL_DISPLAY              0xA6
#define SSD1306_CMD_SET_INVERSE_DISPLAY             0xA7
#define SSD1306_CMD_SET_MULTIPLEX_RATIO             0xA8
#define SSD1306_CMD_SET_DISPLAY_ON                  0xAF
#define SSD1306_CMD_SET_DISPLAY_OFF                 0xAE
#define SSD1306_CMD_SET_PAGE_START_ADDRESS(page)    (0xB0 | (page & 0x07))
#define SSD1306_CMD_SET_COM_OUTPUT_SCAN_UP          0xC0
#define SSD1306_CMD_SET_COM_OUTPUT_SCAN_DOWN        0xC8
#define SSD1306_CMD_SET_DISPLAY_OFFSET              0xD3
#define SSD1306_CMD_SET_DISPLAY_CLOCK_DIVIDE_RATIO  0xD5
#define SSD1306_CMD_SET_PRE_CHARGE_PERIOD           0xD9
#define SSD1306_CMD_SET_COM_PINS                    0xDA
#define SSD1306_CMD_SET_VCOMH_DESELECT_LEVEL        0xDB
#define SSD1306_CMD_NOP                             0xE3
//@}
//! \name Graphic Acceleration Command defines
//@{
#define SSD1306_CMD_SCROLL_H_RIGHT                  0x26
#define SSD1306_CMD_SCROLL_H_LEFT                   0x27
#define SSD1306_CMD_CONTINUOUS_SCROLL_V_AND_H_RIGHT 0x29
#define SSD1306_CMD_CONTINUOUS_SCROLL_V_AND_H_LEFT  0x2A
#define SSD1306_CMD_DEACTIVATE_SCROLL               0x2E
#define SSD1306_CMD_ACTIVATE_SCROLL                 0x2F
#define SSD1306_CMD_SET_VERTICAL_SCROLL_AREA        0xA3
//@}

#define SSD1306_LATENCY 10

#define ssd1306_reset_clear()    gpio_set_level(SSD1306_PIN_NUM_RST, 0)
#define ssd1306_reset_set()      gpio_set_level(SSD1306_PIN_NUM_RST, 1)

// Data/CMD select 
#define ssd1306_sel_data()       gpio_set_level(SSD1306_PIN_NUM_DC, 1)
#define ssd1306_sel_cmd()        gpio_set_level(SSD1306_PIN_NUM_DC, 0)


void ssd1306_spi_write_single(uint8_t data);
void ssd1306_spi_write(uint8_t *data, unsigned short len);

static inline void ssd1306_select_device(void) 
{
//	gpio_set_level(SSD1306_PIN_NUM_CS, 0);
}

static inline void ssd1306_deselect_device(void) 
{
//	gpio_set_level(SSD1306_PIN_NUM_CS, 1);
}


//! \name OLED controller write and read functions
//@{
/**
 * \brief Writes a command to the display controller
 *
 * This functions pull pin D/C# low before writing to the controller. Different
 * data write function is called based on the selected interface.
 *
 * \param command the command to write
 */
static void ssd1306_write_command(uint8_t command)
{
	SPITake(NULL);
	ssd1306_select_device();
	ssd1306_sel_cmd();
	ssd1306_spi_write_single(command);
	ssd1306_deselect_device();
	SPIGive(NULL);
}

/**
 * \brief Write data to the display controller
 *
 * This functions sets the pin D/C# before writing to the controller. Different
 * data write function is called based on the selected interface.
 *
 * \param data the data to write
 */
static /* inline */ void ssd1306_write_data(uint8_t data)
{
	SPITake(NULL);
	ssd1306_select_device();
	ssd1306_sel_data();
	ssd1306_spi_write_single(data);
	ssd1306_deselect_device();
	SPIGive(NULL);

}

static /* inline */ void ssd1306_write(uint8_t *data, unsigned short len)
{
	SPITake(NULL);
	ssd1306_select_device();
	ssd1306_sel_data();
	ssd1306_spi_write(data, len);
	ssd1306_deselect_device();
	SPIGive(NULL);

}

//! \name OLED Controller reset
//@{

/**
 * \brief Perform a hard reset of the OLED controller
 *
 * This functions will reset the OLED controller by setting the reset pin low.
 * \note this functions should not be confused with the \ref ssd1306_soft_reset()
 * function, this command will control the RST pin.
 */
static void ssd1306_hard_reset(void)
{

	gpio_set_direction(SSD1306_PIN_NUM_DC, GPIO_MODE_OUTPUT);
	gpio_set_direction(SSD1306_PIN_NUM_RST, GPIO_MODE_OUTPUT);

	ssd1306_reset_clear();
    	vTaskDelay(100 / portTICK_RATE_MS);
	ssd1306_reset_set();
    	vTaskDelay(100 / portTICK_RATE_MS);
}
//@}

//! \name Sleep control
//@{
/**
 * \brief Enable the OLED sleep mode
 */
static inline void ssd1306_sleep_enable(void)
{
	ssd1306_write_command(SSD1306_CMD_SET_DISPLAY_OFF);
}

/**
 * \brief Disable the OLED sleep mode
 */
static inline void ssd1306_sleep_disable(void)
{
	ssd1306_write_command(SSD1306_CMD_SET_DISPLAY_ON);
}
//@}

//! \name Address setup for the OLED
//@{
/**
 * \brief Set current page in display RAM
 *
 * This command is usually followed by the configuration of the column address
 * because this scheme will provide access to all locations in the display
 * RAM.
 *
 * \param address the page address
 */
static inline void ssd1306_set_page_address(uint8_t address)
{
	// Make sure that the address is 4 bits (only 8 pages)
	address &= 0x0F;
	ssd1306_write_command(SSD1306_CMD_SET_PAGE_START_ADDRESS(address));
}

/**
 * \brief Set current column in display RAM
 *
 * \param address the column address
 */
static inline void ssd1306_set_column_address(uint8_t address)
{
	// Make sure the address is 7 bits
	address &= 0x7F;
	ssd1306_write_command(SSD1306_CMD_SET_HIGH_COL(address >> 4));
	ssd1306_write_command(SSD1306_CMD_SET_LOW_COL(address & 0x0F));
}

/**
 * \brief Set the display start draw line address
 *
 * This function will set which line should be the start draw line for the OLED.
 */
static inline void ssd1306_set_display_start_line_address(uint8_t address)
{
	// Make sure address is 6 bits
	address &= 0x3F;
	ssd1306_write_command(SSD1306_CMD_SET_START_LINE(address));
}
//@}

//! \name Display hardware control
//@{
/**
 * \brief Turn the OLED display on
 *
 * This function will turn on the OLED.
 */
static inline void ssd1306_display_on(void)
{
	ssd1306_write_command(SSD1306_CMD_SET_DISPLAY_ON);
}

/**
 * \brief Turn the OLED display off
 *
 * This function will turn off the OLED.
 */
static inline void ssd1306_display_off(void)
{
	ssd1306_write_command(SSD1306_CMD_SET_DISPLAY_OFF);
}

/**
 * \brief Set the OLED contrast level
 *
 * \param contrast a number between 0 and 0xFF
 *
 * \retval contrast the contrast value written to the OLED controller
 */
static inline uint8_t ssd1306_set_contrast(uint8_t contrast)
{
	ssd1306_write_command(SSD1306_CMD_SET_CONTRAST_CONTROL_FOR_BANK0);
	ssd1306_write_command(contrast);
	return contrast;
}

/**
 * \brief Invert all pixels on the device
 *
 * This function will invert all pixels on the OLED
 *
 */
static inline void ssd1306_display_invert_enable(void)
{
	ssd1306_write_command(SSD1306_CMD_SET_INVERSE_DISPLAY);
}

/**
 * \brief Disable invert of all pixels on the device
 *
 * This function will disable invert on all pixels on the OLED
 *
 */
static inline void ssd1306_display_invert_disable(void)
{
	ssd1306_write_command(SSD1306_CMD_SET_NORMAL_DISPLAY);
}

#if 0
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
#endif
//@}

//! \name Initialization
//@{
void ssd1306_init(void);
//@}

//! \name Write text routine
//@{
void ssd1306_write_text(const char *string);
//@}

/** @} */

#endif /* COMPONENTS_BUS_SPI_INCLUDE_SSD1306_H_ */
