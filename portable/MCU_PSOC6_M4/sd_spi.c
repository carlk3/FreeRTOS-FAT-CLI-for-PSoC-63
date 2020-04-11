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

/* Standard includes. */
#include <stdint.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOSFATConfig.h" // for DBG_PRINTF

#include "project.h"

#include "spi.h"
#include "sd_card.h"

// Lock the SPI and set SS appropriately for this SD Card
// Note: The Cy_SCB_SPI_SetActiveSlaveSelect really only needs to be done here if multiple SDs are on the same SPI.
void sd_spi_acquire(sd_card_t *this) {
	xSemaphoreTakeRecursive(this->spi->mutex, portMAX_DELAY);
	this->spi->owner = xTaskGetCurrentTaskHandle();

	/* Function Name: Cy_SCB_SPI_SetActiveSlaveSelect */
	/* Set active slave select line */
	// Note:
	// Calling this function when the SPI is busy (master preforms data transfer
	//  or slave communicates with the master) may cause transfer corruption
	//  because the hardware stops driving the outputs and ignores the inputs.
	//  Ensure that the SPI is not busy before calling this function.
//    while (Cy_SCB_SPI_IsBusBusy(this->spi->base));
	Cy_SCB_SPI_SetActiveSlaveSelect(this->spi->base, this->ss);
}
void sd_spi_release(sd_card_t *this) {
	this->spi->owner = 0;
	xSemaphoreGiveRecursive(this->spi->mutex);
}

void sd_spi_go_high_frequency(sd_card_t *this) {
	// 100 kHz:
	// In TopDesign.cysch, initial clock setting is
	// 	 Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, 1u, 124u);
	// as seen in cyfitter_cfg.c
	// 400 kHz:
	// 	 Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, 0u, 30u);
	// For 20 MHz:
	//   Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, 0u, 0u);
	// HOWEVER: "Actual data rate (kbps): 12500
	//   (with 4X oversample (the minimum).
	// For 1 Mhz:
	//  UserSDCrd_SPI_SCBCLK_SetDivider(12u);
	// For 6 Mhz:
	//  UserSDCrd_SPI_SCBCLK_SetDivider(1u);

	// For 12.5 MHz:  0    
	const static uint32_t div = 2; //FIXME

	sd_spi_acquire(this);

	if (div == Cy_SysClk_PeriphGetDivider(this->spi->dividerType,
					this->spi->dividerNum)) {
		// Nothing to do
		sd_spi_release(this);
		return;
	}
	Cy_SCB_SPI_Disable(this->spi->base, this->spi->context);
	cy_en_sysclk_status_t rc;
	rc = Cy_SysClk_PeriphDisableDivider(this->spi->dividerType,
			this->spi->dividerNum);
	configASSERT(CY_SYSCLK_SUCCESS == rc);
	rc = Cy_SysClk_PeriphSetDivider(this->spi->dividerType,
			this->spi->dividerNum, div);
	configASSERT(CY_SYSCLK_SUCCESS == rc);
	rc = Cy_SysClk_PeriphEnableDivider(this->spi->dividerType,
			this->spi->dividerNum);
	configASSERT(CY_SYSCLK_SUCCESS == rc);
	Cy_SCB_SPI_Enable(this->spi->base);

	sd_spi_release(this);
}
void sd_spi_go_low_frequency(sd_card_t *this) {
	// In TopDesign.cysch, initial clock setting (for 100 kHz) is
	// 	 Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, 1u, 124u);
	// as seen in cyfitter_cfg.c.
	// For 100 kHz:
	//   UserSDCrd_SPI_SCBCLK_SetDivider(124u);
	// 400 kHz: 30u
	const static uint32_t div = 124u;

	sd_spi_acquire(this);

	if (div == Cy_SysClk_PeriphGetDivider(this->spi->dividerType,
					this->spi->dividerNum)) {
		sd_spi_release(this);
		return;
	}
	Cy_SCB_SPI_Disable(this->spi->base, this->spi->context);
	cy_en_sysclk_status_t rc;
	rc = Cy_SysClk_PeriphDisableDivider(this->spi->dividerType,
			this->spi->dividerNum);
	configASSERT(CY_SYSCLK_SUCCESS == rc);
	rc = Cy_SysClk_PeriphSetDivider(this->spi->dividerType,
			this->spi->dividerNum, div);
	configASSERT(CY_SYSCLK_SUCCESS == rc);
	rc = Cy_SysClk_PeriphEnableDivider(this->spi->dividerType,
			this->spi->dividerNum);
	configASSERT(CY_SYSCLK_SUCCESS == rc);
	Cy_SCB_SPI_Enable(this->spi->base);

	sd_spi_release(this);
}

// SPI Transfer: Read & Write (simultaneously) on SPI bus
//   If the data that will be received is not important, pass NULL as rx.
//   If the data that will be transmitted is not important,
//     pass NULL as tx and then the CY_SCB_SPI_DEFAULT_TX is sent out as each data element.
bool sd_spi_transfer(sd_card_t *this, const uint8_t *tx, uint8_t *rx, size_t length) {
	cy_en_scb_spi_status_t errorStatus;
	uint32_t masterStatus;
	BaseType_t rc;

	sd_spi_acquire(this);

	/* Ensure this task does not already have a notification pending by calling
	 ulTaskNotifyTake() with the xClearCountOnExit parameter set to pdTRUE, and a block time of 0
	 (don't block). */
	rc = ulTaskNotifyTake(pdTRUE, 0);
	configASSERT(!rc);

	/* Initiate SPI Master write and read transaction. */
	errorStatus = Cy_SCB_SPI_Transfer(this->spi->base, (void *) tx, rx, length, this->spi->context);

	/* If no error wait till master sends data in Tx FIFO */
	if ((errorStatus != CY_SCB_SPI_SUCCESS)
			&& (errorStatus != CY_SCB_SPI_TRANSFER_BUSY)) {
		DBG_PRINTF("Cy_SCB_SPI_Transfer failed. Status: 0x%02x\n", errorStatus);
		configASSERT(false);
		sd_spi_release(this);
		return false;
	}

	/* Timeout 1 sec */
	uint32_t timeOut = 1000UL;

	/* Wait until master completes transfer or time out has occured */
	// uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait );   
	rc = ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(timeOut)); // Wait for notification from ISR
	if (!rc) {
		// This indicates that xTaskNotifyWait() returned without the calling task receiving a task notification.
		// The calling task will have been held in the Blocked state to wait for its notification state to become pending, 
		// but the specified block time expired before that happened.
		DBG_PRINTF("Task %s timed out\n",
				pcTaskGetName(xTaskGetCurrentTaskHandle()));
		Cy_SCB_SPI_AbortTransfer(this->spi->base, this->spi->context);
		Cy_SCB_SPI_ClearRxFifo(this->spi->base);
		return false;
	}
	while (CY_SCB_SPI_TRANSFER_ACTIVE
			& Cy_SCB_SPI_GetTransferStatus(this->spi->base, this->spi->context))
		; // Spin
	masterStatus = Cy_SCB_SPI_GetTransferStatus(this->spi->base, this->spi->context);
	if (MASTER_ERROR_MASK & masterStatus) {
		DBG_PRINTF("SPI_Transfer failed. Status: 0x%02lx\n", masterStatus);
		configASSERT(!(MASTER_ERROR_MASK & masterStatus));
		sd_spi_release(this);
		return false;
	}
	uint32_t nxf = Cy_SCB_SPI_GetNumTransfered(this->spi->base, this->spi->context);
	if (nxf != length) {
		DBG_PRINTF("SPI_Transfer failed. length=%u NumTransfered=%lu.\n", length, nxf);
		sd_spi_release(this);
		configASSERT(false);
		return false;
	}
	// There should be no more notifications pending:
	configASSERT(!ulTaskNotifyTake(pdTRUE, 0));

	sd_spi_release(this);
	return true;
}

uint8_t sd_spi_write(sd_card_t *this, const uint8_t value) {
	uint8_t received = 0xFF;

	// static bool sd_spi_transfer(sd_t *this, const uint8_t *tx, uint8_t *rx, size_t length)
	bool rc = sd_spi_transfer(this, (void *) &value, &received, 1);
	configASSERT(rc);

	return received;
}

bool sd_spi_write_block(sd_card_t *this, const uint8_t *tx_buffer, size_t length) {

	// bool spi_transfer(const uint8_t *tx, uint8_t *rx, size_t length)
	bool ret = sd_spi_transfer(this, tx_buffer, NULL, length);

	return ret;
}

/* [] END OF FILE */
