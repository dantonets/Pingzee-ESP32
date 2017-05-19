/*
 * board.h
 *
 *  Created on: May 12, 2017
 *      Author: beam
 */

#ifndef COMPONENTS_APPLICATION_INCLUDE_BOARD_H_
#define COMPONENTS_APPLICATION_INCLUDE_BOARD_H_

#define AT30TSE758_SENSOR_ADDR  0x4f    /*!< slave address for AT30TSE758 temp sensor */
#define AT30TSE758_EEPROM_ADDR  0x57    /*!< slave address for AT30TSE758 EEPROM */

// TODO:	Move this to   the Konfig ..
#define CONFIG_SSD1306_OLED

#ifdef CONFIG_SSD1306_OLED

#define SSD1306_PIN_NUM_CS   5
#define SSD1306_PIN_NUM_DC   26
#define SSD1306_PIN_NUM_RST  27
#endif

#endif /* COMPONENTS_APPLICATION_INCLUDE_BOARD_H_ */
