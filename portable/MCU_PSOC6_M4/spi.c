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
