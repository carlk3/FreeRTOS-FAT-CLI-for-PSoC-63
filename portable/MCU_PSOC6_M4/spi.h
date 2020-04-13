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
#ifndef _SPI_H_
#define _SPI_H_

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include <task.h>
#include <semphr.h>

#include "project.h"

/* Combine SPI master error statuses in single mask */
#define MASTER_ERROR_MASK  (CY_SCB_SPI_SLAVE_TRANSFER_ERR  | CY_SCB_SPI_TRANSFER_OVERFLOW    | \
							CY_SCB_SPI_TRANSFER_UNDERFLOW)  

// "Class" representing SPIs
typedef struct {
	CySCB_Type * const base;
	cy_stc_scb_spi_context_t * const context;
	cy_stc_scb_spi_config_t const *config;
	const cy_en_divider_types_t dividerType; // Clock divider for setting SPI frequency
	const uint32_t dividerNum;    // Clock divider for setting SPI frequency  
	GPIO_PRT_Type * const spi_miso_port; // GPIO port for SPI pins
	const uint32_t spi_miso_num;   // SPI MISO pin number for GPIO    
	const cy_stc_sysint_t * const intcfg;
	const cy_israddress userIsr;
	const cy_cb_scb_spi_handle_events_t callback;
	bool initialized;   // Assigned dynamically        
	TaskHandle_t owner; // Assigned dynamically      
	SemaphoreHandle_t mutex; // Assigned dynamically      
} spi_t;

bool spi_init(spi_t *pSD);

// SPI Interrupt callback
void SPI_handle_event(spi_t *this, uint32_t event);

#endif
/* [] END OF FILE */
