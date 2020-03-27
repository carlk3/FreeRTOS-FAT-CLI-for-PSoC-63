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

#ifndef _SD_SPI_H_
#define _SD_SPI_H_

#include <stdint.h>
#include <sd_card.h>
    
#define SPI_FILL_CHAR         (0xFF)  

bool sd_spi_transfer(sd_t *this, const uint8_t *tx, uint8_t *rx, size_t length);
uint8_t sd_spi_write(sd_t *this, const uint8_t value);
bool sd_spi_write_block(sd_t *this, const uint8_t *tx_buffer, size_t length);
void sd_spi_acquire(sd_t *this);
void sd_spi_release(sd_t *this);
void sd_spi_go_low_frequency(sd_t *this);
void sd_spi_go_high_frequency(sd_t *this);

#endif

/* [] END OF FILE */
