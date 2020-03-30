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
/* 

This file should be tailored to match the hardware design that is specified in 
TopDesign.cysch and Design Wide Resources/Pins (the .cydwr file).

There should be one element of the spi[] array for each hardware SPI, and the 
name prefix (e.g.: "SPI_1") will depend on the name given in TopDesign.
Each SPI needs its own interrupt callback function that specifies the correct 
element in the spi[] array. E.g.: spi[0]. 

There should be one element of the sd_cards[] array for each SD card slot. 
(There is no corresponding component in TopDesign.) 
The name is arbitrary. The rest of the constants will depend on the type of socket,
which SPI it is driven by, and how it is wired.

*/


#include <string.h>
#include <assert.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"

// Make it easier to spot errors:
#include "project.h" 
#include "hw_config.h"

//#define DBG_PRINTF(fmt, args...) /* Don't do anything */
extern void my_printf(const char *pcFormat, ...) __attribute__ ((format (printf, 1, 2)));
#define DBG_PRINTF my_printf

// SPI Interrupt callbacks
void SPI_1_handle_event(uint32_t event);

// Hardware Configuration of SPI "objects"
// Note: multiple SD cards can be driven by one SPI if they use different slave selects.
static spi_t spi[] = { // One for each SPI. 
		{
			.base = SPI_1_HW, // SPI component
			.context = &SPI_1_context, // SPI component
			.config = &SPI_1_config, // SPI component
			.dividerType = SPI_1_SCBCLK_DIV_TYPE, // Clock divider for setting frequency
			.dividerNum = SPI_1_SCBCLK_DIV_NUM, // Clock divider for setting frequency
			.spi_miso_port = SPI_1_miso_m_0_PORT, // Refer to Design Wide Resouces, Pins, in the .cydwr file
			.spi_miso_num = SPI_1_miso_m_0_NUM, // MISO GPIO pin number for pull-up (see cyfitter_gpio.h)
			.intcfg = &SPI_1_SCB_IRQ_cfg, // Interrupt
			.userIsr = &SPI_1_Interrupt, // Interrupt
			.callback = SPI_1_handle_event, // Interrupt callback
			.initialized = false, //initialized flag
			.owner = 0, // Owning task, assigned dynamically
			.mutex = 0  // Guard semaphore, assigned dynamically
		}
};

// Hardware Configuration of the SD Card "objects"
static sd_t sd_cards[] = { 	// One for each SD card
	{ 
			.pcName = "SDCard",   // Name used to mount device
			.spi = &spi[0], // Pointer to the SPI driving this card
			.ss = SPI_1_SPI_SLAVE_SELECT0, // The SPI slave select line for this SD card
			.card_detect_gpio_port = Card_Detect_PORT, // Card detect
			.card_detect_gpio_num = Card_Detect_NUM, // Card detect
			.card_detected_true = 0, // truth (card is present) is 0 
			.initialized = false, 
			.m_Status = 0, 
			.sectors = 0, 
			.card_type = 0, 
			.mutex = 0
	}	
};


// Callback function called in the Cy_SCB_SPI_Interrupt to notify the user about occurrences of SPI Callback Events.
// Each SPI has its own interrupt and callback.
// Notifies task when a transfer is complete.
void SPI_1_handle_event(uint32_t event) {
	// CY_SCB_SPI_TRANSFER_CMPLT_EVENT   (0x02U)
	// The transfer operation started by Cy_SCB_SPI_Transfer is complete.
	if (CY_SCB_SPI_TRANSFER_CMPLT_EVENT == event) {
		/* The xHigherPriorityTaskWoken parameter must be initialized to pdFALSE as
		 it will get set to pdTRUE inside the interrupt safe API function if a
		 context switch is required. */
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		/* Send a notification directly to the task to which interrupt processing is
		 being deferred. */
		configASSERT(spi[0].owner);
		if (spi[0].owner) {
			vTaskNotifyGiveFromISR(spi[0].owner, &xHigherPriorityTaskWoken);
		}
		/* Pass the xHigherPriorityTaskWoken value into portYIELD_FROM_ISR(). If
		 xHigherPriorityTaskWoken was set to pdTRUE inside vTaskNotifyGiveFromISR()
		 then calling portYIELD_FROM_ISR() will request a context switch. If
		 xHigherPriorityTaskWoken is still pdFALSE then calling
		 portYIELD_FROM_ISR() will have no effect. */
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	} else {
		if (MASTER_ERROR_MASK & event) {
			DBG_PRINTF("SPI_1_handle_event. Event: 0x%lx\n", event);
			assert(!(MASTER_ERROR_MASK & event));
		}
	}
}

sd_t *sd_get_by_name(const char *const name) {
		size_t i;
		for (i = 0; i < sizeof(sd_cards) / sizeof(sd_cards[0]); ++i) {
			if (0 == strcmp(sd_cards[i].pcName, name))
				break;
		}
		if (sizeof(sd_cards) / sizeof(sd_cards[0]) == i) {
			DBG_PRINTF("FF_SDDiskInit: unknown name %s\n", name);
			return NULL;
		}
		return &sd_cards[i];
}        

sd_t *sd_get_by_num(size_t num) {
	if (num <= sizeof(sd_cards) / sizeof(sd_cards[0])) {
		return &sd_cards[num];
	} else {
		return NULL;
	}
}        


/* [] END OF FILE */
