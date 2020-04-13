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

#include "project.h"
#include "spi.h"

#include "FreeRTOSFATConfig.h" // for DBG_PRINTF

/* Combine SPI master error statuses in single mask */
#define MASTER_ERROR_MASK  (CY_SCB_SPI_SLAVE_TRANSFER_ERR  | CY_SCB_SPI_TRANSFER_OVERFLOW    | \
							CY_SCB_SPI_TRANSFER_UNDERFLOW)  

void SPI_handle_event(spi_t *this, uint32_t event) {
	// CY_SCB_SPI_TRANSFER_CMPLT_EVENT   (0x02U)
	// The transfer operation started by Cy_SCB_SPI_Transfer is complete.
	if (CY_SCB_SPI_TRANSFER_CMPLT_EVENT == event) {
		/* The xHigherPriorityTaskWoken parameter must be initialized to pdFALSE as
		 it will get set to pdTRUE inside the interrupt safe API function if a
		 context switch is required. */
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		/* Send a notification directly to the task to which interrupt processing is
		 being deferred. */
		configASSERT(this->owner);
		vTaskNotifyGiveFromISR(this->owner, // The handle of the task to which the notification is being sent.
				&xHigherPriorityTaskWoken);
		/* Pass the xHigherPriorityTaskWoken value into portYIELD_FROM_ISR(). If
		 xHigherPriorityTaskWoken was set to pdTRUE inside vTaskNotifyGiveFromISR()
		 then calling portYIELD_FROM_ISR() will request a context switch. If
		 xHigherPriorityTaskWoken is still pdFALSE then calling
		 portYIELD_FROM_ISR() will have no effect. */
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	} else {
		if (MASTER_ERROR_MASK & event) {
			DBG_PRINTF("SysSDCrd0_SPI_handle_event. Event: 0x%lx\n", (unsigned long)event);
			configASSERT(!(MASTER_ERROR_MASK & event));            
		}
	}
}

bool spi_init(spi_t *this) {
	cy_en_scb_spi_status_t initStatus;
	cy_en_sysint_status_t sysSpistatus;   

	// bool __atomic_test_and_set (void *ptr, int memorder)
	// This built-in function performs an atomic test-and-set operation on the byte at *ptr. 
	// The byte is set to some implementation defined nonzero “set” value 
	// and the return value is true if and only if the previous contents were “set”. 
	if (__atomic_test_and_set(&(this->initialized), __ATOMIC_SEQ_CST))
		return true;

	/* Configure component */
	initStatus = Cy_SCB_SPI_Init(this->base, this->config, this->context);
	if (initStatus != CY_SCB_SPI_SUCCESS) {
		DBG_PRINTF("FAILED_OPERATION: Cy_SCB_SPI_Init\n");
		return false;
	}
	//  void Cy_SCB_SPI_RegisterCallback	(	CySCB_Type const * 	base,
	//  cy_cb_scb_spi_handle_events_t 	callback,
	//  cy_stc_scb_spi_context_t * 	context 
	//  )		
	Cy_SCB_SPI_RegisterCallback(this->base, this->callback, this->context);

	/* Hook interrupt service routine */
	sysSpistatus = Cy_SysInt_Init(this->intcfg, this->userIsr);
	if (sysSpistatus != CY_SYSINT_SUCCESS) {
		DBG_PRINTF("FAILED_OPERATION: Cy_SysInt_Init\n");
		return false;
	}
	/* Enable interrupt in NVIC */
	NVIC_EnableIRQ(this->intcfg->intrSrc);
	/* Clear any pending interrupts */
	__NVIC_ClearPendingIRQ(this->intcfg->intrSrc);

	// SD cards' DO MUST be pulled up.
	Cy_GPIO_SetDrivemode(this->spi_miso_port, this->spi_miso_num, CY_GPIO_DM_PULLUP);

	// Do this here if not done in sd_spi_acquire:
//	Cy_SCB_SPI_SetActiveSlaveSelect(this->base, this->ss);

	/* Enable SPI master hardware. */
	Cy_SCB_SPI_Enable(this->base);

	// The SPI may be shared (using multiple SSs); protect it
	this->mutex = xSemaphoreCreateRecursiveMutex();

	return true;
}


/* [] END OF FILE */
