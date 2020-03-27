/* ========================================
 * ========================================
*/

#ifdef _FF_SDDISK_CONFIG_H_
#error This file should be included exactly once!
#define _FF_SDDISK_CONFIG_H_
#endif

#include <stdio.h>  // printf
#include <assert.h>
 
/* FreeRTOS includes. */
#include "FreeRTOS.h"

// Make it easier to spot errors:
#include "project.h" 
#include "sd_card.h"

// SPI Interrupt callbacks
void SPI_1_handle_event(uint32_t event);

// Hardware Configuration of SPI "objects"
// Note that multiple SD cards can be on one SPI if they use different slave selects.
static spi_t spi[] = { // One for each SPI. 
		{
			SPI_1_HW, // SPI component
			&SPI_1_context, // SPI component
			&SPI_1_config, // SPI component
			SPI_1_SCBCLK_DIV_TYPE, // Clock divider for setting frequency
			SPI_1_SCBCLK_DIV_NUM, // Clock divider for setting frequency
			SPI_1_miso_m_0_PORT, // Refer to Design Wide Resouces, Pins, in the .cydwr file
			SPI_1_miso_m_0_NUM, // MISO GPIO pin number for pull-up (see cyfitter_gpio.h)
			&SPI_1_SCB_IRQ_cfg, // Interrupt
			&SPI_1_Interrupt, // Interrupt
			SPI_1_handle_event, // Interrupt callback
            0, //initialized flag
			0, // Owning task, assigned dynamically
			0  // Guard semaphore, assigned dynamically
		}
};

// Hardware Configuration of the SD Card "objects"
static sd_t sd_cards[] = { 	// One for each SD card
	{ 
			"SDCard",   // Name used to mount device
			&spi[0], // Pointer to the SPI driving this card
    		SPI_1_SPI_SLAVE_SELECT0, // The SPI slave select line for this SD card
			Card_Detect_PORT, // Card detect
			Card_Detect_NUM, // Card detect
			0, // truth (card is present) is 0 
			0, 0, 0, 0, 0
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
		vTaskNotifyGiveFromISR(spi[0].owner, // The handle of the task to which the notification is being sent.
				&xHigherPriorityTaskWoken);
		/* Pass the xHigherPriorityTaskWoken value into portYIELD_FROM_ISR(). If
		 xHigherPriorityTaskWoken was set to pdTRUE inside vTaskNotifyGiveFromISR()
		 then calling portYIELD_FROM_ISR() will request a context switch. If
		 xHigherPriorityTaskWoken is still pdFALSE then calling
		 portYIELD_FROM_ISR() will have no effect. */
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	} else {
		if (MASTER_ERROR_MASK & event) {
			printf("SPI_1_handle_event. Event: 0x%lx\n", event);
            assert(!(MASTER_ERROR_MASK & event));
		}
	}
}


/* [] END OF FILE */
