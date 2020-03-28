/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/

#ifndef _SD_CARD_H_
#define _SD_CARD_H_

#include <stdint.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include <semphr.h>

#include "project.h"
#include "spi.h"    

// "Class" representing SD Cards
typedef struct {
	const char *pcName;
	spi_t * const spi;
	// Slave select is here in sd_t because multiple SDs can share an SPI
	const cy_en_scb_spi_slave_select_t ss; // Slave select for this SD card
	GPIO_PRT_Type * const card_detect_gpio_port; // Card detect
	const uint32_t card_detect_gpio_num; // Card detect
	const uint32_t card_detected_true; // 0 for full-size SD, 1 for micro
	bool initialized;   // Assigned dynamically    
	int m_Status; // Card status
	uint64_t sectors;   // Assigned dynamically
	int card_type;  // Assigned dynamically
	SemaphoreHandle_t mutex; // Guard semaphore, assigned dynamically
} sd_t;

#define SD_BLOCK_DEVICE_ERROR_NONE                   0
#define SD_BLOCK_DEVICE_ERROR_WOULD_BLOCK        -5001  /*!< operation would block */
#define SD_BLOCK_DEVICE_ERROR_UNSUPPORTED        -5002  /*!< unsupported operation */
#define SD_BLOCK_DEVICE_ERROR_PARAMETER          -5003  /*!< invalid parameter */
#define SD_BLOCK_DEVICE_ERROR_NO_INIT            -5004  /*!< uninitialized */
#define SD_BLOCK_DEVICE_ERROR_NO_DEVICE          -5005  /*!< device is missing or not connected */
#define SD_BLOCK_DEVICE_ERROR_WRITE_PROTECTED    -5006  /*!< write protected */
#define SD_BLOCK_DEVICE_ERROR_UNUSABLE           -5007  /*!< unusable card */
#define SD_BLOCK_DEVICE_ERROR_NO_RESPONSE        -5008  /*!< No response from device */
#define SD_BLOCK_DEVICE_ERROR_CRC                -5009  /*!< CRC error */
#define SD_BLOCK_DEVICE_ERROR_ERASE              -5010  /*!< Erase error: reset/sequence */
#define SD_BLOCK_DEVICE_ERROR_WRITE              -5011  /*!< SPI Write error: !SPI_DATA_ACCEPTED */

sd_t *sd_get_by_name(const char *const name);
sd_t *sd_get_by_num(size_t num);
int sd_driver_init(sd_t *this);
int sd_write_blocks(sd_t *this, const uint8_t *buffer, uint64_t ulSectorNumber, uint32_t blockCnt);
int sd_read_blocks(sd_t *this, uint8_t *buffer, uint64_t ulSectorNumber, uint32_t ulSectorCount);
bool sd_card_detect(sd_t *this);
uint64_t sd_sectors(sd_t *this);

#endif
/* [] END OF FILE */