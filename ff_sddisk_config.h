/* ========================================
 * ========================================
*/

#ifdef _FF_SDDISK_CONFIG_H_
#error This file should be included exactly once!
#define _FF_SDDISK_CONFIG_H_
#endif

#include <stdio.h>  // printf
#include <stdarg.h> // varargs

// Make it easier to spot errors:
#include "project.h" 
#include "FreeRTOS.h"
#include "FreeRTOSFATConfig.h"
#include <task.h>
#include <semphr.h>
#include <event_groups.h>


static void my_printf(const char *pcFormat, ...) {
	char pcBuffer[256] = { 0 };
	va_list xArgs;
	va_start(xArgs, pcFormat);
	vsnprintf(pcBuffer, sizeof(pcBuffer), pcFormat, xArgs);
	va_end(xArgs); FF_PRINTF("%p, %s: %s", xTaskGetCurrentTaskHandle(),
			pcTaskGetName(xTaskGetCurrentTaskHandle()), pcBuffer);
}

#define DBG_PRINTF my_printf
//#define DBG_PRINTF(fmt, args...)    FF_PRINTF(fmt, ## args)
//#define DBG_PRINTF(fmt, args...) /* Don't do anything */

// SPI Interrupt callbacks
void SPI_1_handle_event(uint32_t event);

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
	const cy_en_scb_spi_slave_select_t ss;
	bool initialized;   // Assigned dynamically        
	TaskHandle_t owner; // Assigned dynamically      
	SemaphoreHandle_t mutex; // Assigned dynamically      
} spi_t;

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
	EventGroupHandle_t ready; // Flag for waiters
	int m_Status; // Card status
	uint64_t sectors;   // Assigned dynamically
	int card_type;  // Assigned dynamically
	SemaphoreHandle_t mutex; // Guard semaphore, assigned dynamically
} sd_t;

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
			SPI_1_SPI_SLAVE_SELECT0, 0, //initialized flag
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
			1, // truth is 1
			0, 0, 0, 0, 0, 0
	}	
};


/* Combine SPI master error statuses in single mask */
#define MASTER_ERROR_MASK  (CY_SCB_SPI_SLAVE_TRANSFER_ERR  | CY_SCB_SPI_TRANSFER_OVERFLOW    | \
							CY_SCB_SPI_TRANSFER_UNDERFLOW)  

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
			DBG_PRINTF("SPI_1_handle_event. Event: 0x%ux\n", event);
		}
	}
}

/* [] END OF FILE */
