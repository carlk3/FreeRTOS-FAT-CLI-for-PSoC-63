/*
 * FreeRTOS+FAT DOS Compatible Embedded FAT File System 
 *     https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_FAT/index.html
 * ported to Cypress CY8C6347BZI-BLD53.
 *
 * This code borrows heavily from the Mbed SDBlockDevice:
 *       https://os.mbed.com/docs/mbed-os/v5.15/apis/sdblockdevice.html
 *       mbed-os/components/storage/blockdevice/COMPONENT_SD/SDBlockDevice.cpp
 *
 * Editor: Carl Kugler (carlk3@gmail.com)
 * 
 * Remember your ABCs: "Always Be Cobbling!"
 */
/*
 * FreeRTOS+FAT build 191128 - Note:  FreeRTOS+FAT is still in the lab!
 * Copyright (C) 2018 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Authors include James Walmsley, Hein Tibosch and Richard Barry
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 *
 */
/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Introduction
 * ------------
 * SD and MMC cards support a number of interfaces, but common to them all
 * is one based on SPI. Since we already have the mbed SPI Interface, it will
 * be used for SD cards.
 *
 * The main reference I'm using is Chapter 7, "SPI Mode" of:
 *  http://www.sdcard.org/developers/tech/sdcard/pls/Simplified_Physical_Layer_Spec.pdf
 *
 * SPI Startup
 * -----------
 * The SD card powers up in SD mode. The start-up procedure is complicated
 * by the requirement to support older SDCards in a backwards compatible
 * way with the new higher capacity variants SDHC and SDHC.
 *
 * The following figures from the specification with associated text describe
 * the SPI mode initialisation process:
 *  - Figure 7-1: SD Memory Card State Diagram (SPI mode)
 *  - Figure 7-2: SPI Mode Initialization Flow
 *
 * Firstly, a low initial clock should be selected (in the range of 100-
 * 400kHZ). After initialisation has been completed, the switch to a
 * higher clock speed can be made (e.g. 1MHz). Newer cards will support
 * higher speeds than the default _transfer_sck defined here.
 *
 * Next, note the following from the SDCard specification (note to
 * Figure 7-1):
 *
 *  In any of the cases CMD1 is not recommended because it may be difficult for the host
 *  to distinguish between MultiMediaCard and SD Memory Card
 *
 * Hence CMD1 is not used for the initialisation sequence.
 *
 * The SPI interface mode is selected by asserting CS low and sending the
 * reset command (CMD0). The card will respond with a (R1) response.
 * In practice many cards initially respond with 0xff or invalid data
 * which is ignored. Data is read until a valid response is received
 * or the number of re-reads has exceeded a maximim count. If a valid
 * response is not received then the CMD0 can be retried. This
 * has been found to successfully initialise cards where the SPI master
 * (on MCU) has been reset but the SDCard has not, so the first
 * CMD0 may be lost.
 *
 * CMD8 is optionally sent to determine the voltage range supported, and
 * indirectly determine whether it is a version 1.x SD/non-SD card or
 * version 2.x. I'll just ignore this for now.
 *
 * ACMD41 is repeatedly issued to initialise the card, until "in idle"
 * (bit 0) of the R1 response goes to '0', indicating it is initialised.
 *
 * You should also indicate whether the host supports High Capicity cards,
 * and check whether the card is high capacity - i'll also ignore this
 *
 * SPI Protocol
 * ------------
 * The SD SPI protocol is based on transactions made up of 8-bit words, with
 * the host starting every bus transaction by asserting the CS signal low. The
 * card always responds to commands, data blocks and errors.
 *
 * The protocol supports a CRC, but by default it is off (except for the
 * first reset CMD0, where the CRC can just be pre-calculated, and CMD8)
 * I'll leave the CRC off I think!
 *
 * Standard capacity cards have variable data block sizes, whereas High
 * Capacity cards fix the size of data block to 512 bytes. I'll therefore
 * just always use the Standard Capacity cards with a block size of 512 bytes.
 * This is set with CMD16.
 *
 * You can read and write single blocks (CMD17, CMD25) or multiple blocks
 * (CMD18, CMD25). For simplicity, I'll just use single block accesses. When
 * the card gets a read command, it responds with a response token, and then
 * a data token or an error.
 *
 * SPI Command Format
 * ------------------
 * Commands are 6-bytes long, containing the command, 32-bit argument, and CRC.
 *
 * +---------------+------------+------------+-----------+----------+--------------+
 * | 01 | cmd[5:0] | arg[31:24] | arg[23:16] | arg[15:8] | arg[7:0] | crc[6:0] | 1 |
 * +---------------+------------+------------+-----------+----------+--------------+
 *
 * As I'm not using CRC, I can fix that byte to what is needed for CMD0 (0x95)
 *
 * All Application Specific commands shall be preceded with APP_CMD (CMD55).
 *
 * SPI Response Format
 * -------------------
 * The main response format (R1) is a status byte (normally zero). Key flags:
 *  idle - 1 if the card is in an idle state/initialising
 *  cmd  - 1 if an illegal command code was detected
 *
 *    +-------------------------------------------------+
 * R1 | 0 | arg | addr | seq | crc | cmd | erase | idle |
 *    +-------------------------------------------------+
 *
 * R1b is the same, except it is followed by a busy signal (zeros) until
 * the first non-zero byte when it is ready again.
 *
 * Data Response Token
 * -------------------
 * Every data block written to the card is acknowledged by a byte
 * response token
 *
 * +----------------------+
 * | xxx | 0 | status | 1 |
 * +----------------------+
 *              010 - OK!
 *              101 - CRC Error
 *              110 - Write Error
 *
 * Single Block Read and Write
 * ---------------------------
 *
 * Block transfers have a byte header, followed by the data, followed
 * by a 16-bit CRC. In our case, the data will always be 512 bytes.
 *
 * +------+---------+---------+- -  - -+---------+-----------+----------+
 * | 0xFE | data[0] | data[1] |        | data[n] | crc[15:8] | crc[7:0] |
 * +------+---------+---------+- -  - -+---------+-----------+----------+
 */

/* Standard includes. */
#include <inttypes.h>
#include <sys/param.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include <task.h>
#include <event_groups.h>

/* FreeRTOS+FAT includes. */
#include "ff_sddisk.h"
#include "ff_sys.h"

#include "project.h"

// Hardware Configuration of the SPI and SD Card "objects"
#include "ff_sddisk_config.h"

/* Disk Status Bits (DSTATUS) */
enum {
	STA_NOINIT = 0x01, /* Drive not initialized */
	STA_NODISK = 0x02, /* No medium in the drive */
	STA_PROTECT = 0x04 /* Write protected */
};

/* Control Tokens   */
#define SPI_DATA_RESPONSE_MASK   (0x1F)
#define SPI_DATA_ACCEPTED        (0x05)
#define SPI_DATA_CRC_ERROR       (0x0B)
#define SPI_DATA_WRITE_ERROR     (0x0D)
#define SPI_START_BLOCK          (0xFE)      /*!< For Single Block Read/Write and Multiple Block Read */
#define SPI_START_BLK_MUL_WRITE  (0xFC)      /*!< Start Multi-block write */
#define SPI_STOP_TRAN            (0xFD)      /*!< Stop Multi-block write */

#define SPI_DATA_READ_ERROR_MASK (0xF)       /*!< Data Error Token: 4 LSB bits */
#define SPI_READ_ERROR           (0x1 << 0)  /*!< Error */
#define SPI_READ_ERROR_CC        (0x1 << 1)  /*!< CC Error*/
#define SPI_READ_ERROR_ECC_C     (0x1 << 2)  /*!< Card ECC failed */
#define SPI_READ_ERROR_OFR       (0x1 << 3)  /*!< Out of Range */

// SPI Slave Select
#define SSEL_ACTIVE      ( 0 )  
#define SSEL_INACTIVE    ( 1 )

/** Represents the different SD/MMC card types  */
// Types
#define SDCARD_NONE              0           /**< No card is present */
#define SDCARD_V1                1           /**< v1.x Standard Capacity */
#define SDCARD_V2                2           /**< v2.x Standard capacity SD card */
#define SDCARD_V2HC              3           /**< v2.x High capacity SD card */
#define CARD_UNKNOWN             4           /**< Unknown or unsupported card */

// Only HC block size is supported. Making this a static constant reduces code size.
#define BLOCK_SIZE_HC                            512    /*!< Block size supported for SD card is 512 bytes  */
static const uint32_t _block_size = BLOCK_SIZE_HC;

/* R1 Response Format */
#define R1_NO_RESPONSE          (0xFF)
#define R1_RESPONSE_RECV        (0x80)
#define R1_IDLE_STATE           (1 << 0)
#define R1_ERASE_RESET          (1 << 1)
#define R1_ILLEGAL_COMMAND      (1 << 2)
#define R1_COM_CRC_ERROR        (1 << 3)
#define R1_ERASE_SEQUENCE_ERROR (1 << 4)
#define R1_ADDRESS_ERROR        (1 << 5)
#define R1_PARAMETER_ERROR      (1 << 6)

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

// Supported SD Card Commands
typedef enum {
	CMD_NOT_SUPPORTED = -1, /**< Command not supported error */
	CMD0_GO_IDLE_STATE = 0, /**< Resets the SD Memory Card */
	CMD1_SEND_OP_COND = 1, /**< Sends host capacity support */
	CMD6_SWITCH_FUNC = 6, /**< Check and Switches card function */
	CMD8_SEND_IF_COND = 8, /**< Supply voltage info */
	CMD9_SEND_CSD = 9, /**< Provides Card Specific data */
	CMD10_SEND_CID = 10, /**< Provides Card Identification */
	CMD12_STOP_TRANSMISSION = 12, /**< Forces the card to stop transmission */
	CMD13_SEND_STATUS = 13, /**< Card responds with status */
	CMD16_SET_BLOCKLEN = 16, /**< Length for SC card is set */
	CMD17_READ_SINGLE_BLOCK = 17, /**< Read single block of data */
	CMD18_READ_MULTIPLE_BLOCK = 18, /**< Card transfers data blocks to host until interrupted
	 by a STOP_TRANSMISSION command */
	CMD24_WRITE_BLOCK = 24, /**< Write single block of data */
	CMD25_WRITE_MULTIPLE_BLOCK = 25, /**< Continuously writes blocks of data until
	 'Stop Tran' token is sent */
	CMD27_PROGRAM_CSD = 27, /**< Programming bits of CSD */
	CMD32_ERASE_WR_BLK_START_ADDR = 32, /**< Sets the address of the first write
	 block to be erased. */
	CMD33_ERASE_WR_BLK_END_ADDR = 33, /**< Sets the address of the last write
	 block of the continuous range to be erased.*/
	CMD38_ERASE = 38, /**< Erases all previously selected write blocks */
	CMD55_APP_CMD = 55, /**< Extend to Applications specific commands */
	CMD56_GEN_CMD = 56, /**< General Purpose Command */
	CMD58_READ_OCR = 58, /**< Read OCR register of card */
	CMD59_CRC_ON_OFF = 59, /**< Turns the CRC option on or off*/
	// App Commands
	ACMD6_SET_BUS_WIDTH = 6,
	ACMD13_SD_STATUS = 13,
	ACMD22_SEND_NUM_WR_BLOCKS = 22,
	ACMD23_SET_WR_BLK_ERASE_COUNT = 23,
	ACMD41_SD_SEND_OP_COND = 41,
	ACMD42_SET_CLR_CARD_DETECT = 42,
	ACMD51_SEND_SCR = 51,
} cmdSupported;

// Lock the SPI and set SS appropriately for this SD Card
// Note: The Cy_SCB_SPI_SetActiveSlaveSelect really only needs to be done here if multiple SDs are on the same SPI.
static void sd_spi_acquire(sd_t *this) {
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
static void sd_spi_release(sd_t *this) {
	this->spi->owner = 0;
	xSemaphoreGiveRecursive(this->spi->mutex);
}

static void sd_spi_go_high_frequency(sd_t *this) {
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
static void sd_spi_go_low_frequency(sd_t *this) {
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
static bool sd_spi_transfer(sd_t *this, const uint8_t *tx, uint8_t *rx, size_t length) {
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
		DBG_PRINTF("Cy_SCB_SPI_Transfer failed. Status: 0x%02lx\n", errorStatus);
		configASSERT(false);
		sd_spi_release(this);
		return false;
	}

	/* Timeout 1 sec */
	uint32_t timeOut = 2000UL; // FIXME

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
		DBG_PRINTF("SPI_Transfer failed. length=%d NumTransfered=%d.\n", length, nxf);
		sd_spi_release(this);
		configASSERT(false);
		return false;
	}
	// There should be no more notifications pending:
	configASSERT(!ulTaskNotifyTake(pdTRUE, 0));

	sd_spi_release(this);
	return true;
}

static uint8_t sd_spi_write(sd_t *this, const uint8_t value) {
	uint8_t received = 0xFF;

	// static bool sd_spi_transfer(sd_t *this, const uint8_t *tx, uint8_t *rx, size_t length)
	bool rc = sd_spi_transfer(this, (void *) &value, &received, 1);
	configASSERT(rc);

	return received;
}

/* SIZE in Bytes */
#define PACKET_SIZE              6           /*!< SD Packet size CMD+ARG+CRC */
#define R1_RESPONSE_SIZE         1           /*!< Size of R1 response */
#define R2_RESPONSE_SIZE         2           /*!< Size of R2 response */
#define R3_R7_RESPONSE_SIZE      5           /*!< Size of R3/R7 response */

/* R3 Response : OCR Register */
#define OCR_HCS_CCS             (0x1 << 30)
#define OCR_LOW_VOLTAGE         (0x01 << 24)
#define OCR_3_3V                (0x1 << 20)

#define SPI_CMD(x) (0x40 | (x & 0x3f))

#define SPI_FILL_CHAR         (0xFF)

static uint8_t sd_cmd_spi(sd_t *this, cmdSupported cmd, uint32_t arg) {
	uint8_t response;
	char cmdPacket[PACKET_SIZE];

	// Prepare the command packet
	cmdPacket[0] = SPI_CMD(cmd);
	cmdPacket[1] = (arg >> 24);
	cmdPacket[2] = (arg >> 16);
	cmdPacket[3] = (arg >> 8);
	cmdPacket[4] = (arg >> 0);

#if MBED_CONF_SD_CRC_ENABLED
	uint32_t crc;
	if (_crc_on) {
		_crc7.compute((void *)cmdPacket, 5, &crc);
		cmdPacket[5] = (char)(crc | 0x01);
	} else
#endif
	{
		// CMD0 is executed in SD mode, hence should have correct CRC
		// CMD8 CRC verification is always enabled
		switch (cmd) {
		case CMD0_GO_IDLE_STATE:
			cmdPacket[5] = 0x95;
			break;
		case CMD8_SEND_IF_COND:
			cmdPacket[5] = 0x87;
			break;
		default:
			cmdPacket[5] = 0xFF;    // Make sure bit 0-End bit is high
			break;
		}
	}
	// send a command
	for (int i = 0; i < PACKET_SIZE; i++) {
		sd_spi_write(this, cmdPacket[i]);
	}
	// The received byte immediataly following CMD12 is a stuff byte,
	// it should be discarded before receive the response of the CMD12.
	if (CMD12_STOP_TRANSMISSION == cmd) {
		sd_spi_write(this, SPI_FILL_CHAR);
	}
	// Loop for response: Response is sent back within command response time (NCR), 0 to 8 bytes for SDC
	for (int i = 0; i < 0x10; i++) {
		response = sd_spi_write(this, SPI_FILL_CHAR);
		// Got the response
		if (!(response & R1_RESPONSE_RECV)) {
			break;
		}
	}
	return response;
}

static bool sd_wait_ready(sd_t *this, int timeout) {
	char resp;

	//Keep sending dummy clocks with DI held high until the card releases the DO line

	TickType_t xStart = xTaskGetTickCount();

	do {
		resp = sd_spi_write(this, 0xFF);
	} while (resp == 0x00
			&& (xTaskGetTickCount() - xStart) < pdMS_TO_TICKS(timeout));

	configASSERT(resp > 0x00);

	//Return success/failure
	return (resp > 0x00);
}

#if MBED_CONF_SD_CRC_ENABLED

static const char m_Crc7Table[] = {0x00, 0x09, 0x12, 0x1B, 0x24, 0x2D, 0x36,
	0x3F, 0x48, 0x41, 0x5A, 0x53, 0x6C, 0x65, 0x7E, 0x77, 0x19, 0x10, 0x0B,
	0x02, 0x3D, 0x34, 0x2F, 0x26, 0x51, 0x58, 0x43, 0x4A, 0x75, 0x7C, 0x67,
	0x6E, 0x32, 0x3B, 0x20, 0x29, 0x16, 0x1F, 0x04, 0x0D, 0x7A, 0x73, 0x68,
	0x61, 0x5E, 0x57, 0x4C, 0x45, 0x2B, 0x22, 0x39, 0x30, 0x0F, 0x06, 0x1D,
	0x14, 0x63, 0x6A, 0x71, 0x78, 0x47, 0x4E, 0x55, 0x5C, 0x64, 0x6D, 0x76,
	0x7F, 0x40, 0x49, 0x52, 0x5B, 0x2C, 0x25, 0x3E, 0x37, 0x08, 0x01, 0x1A,
	0x13, 0x7D, 0x74, 0x6F, 0x66, 0x59, 0x50, 0x4B, 0x42, 0x35, 0x3C, 0x27,
	0x2E, 0x11, 0x18, 0x03, 0x0A, 0x56, 0x5F, 0x44, 0x4D, 0x72, 0x7B, 0x60,
	0x69, 0x1E, 0x17, 0x0C, 0x05, 0x3A, 0x33, 0x28, 0x21, 0x4F, 0x46, 0x5D,
	0x54, 0x6B, 0x62, 0x79, 0x70, 0x07, 0x0E, 0x15, 0x1C, 0x23, 0x2A, 0x31,
	0x38, 0x41, 0x48, 0x53, 0x5A, 0x65, 0x6C, 0x77, 0x7E, 0x09, 0x00, 0x1B,
	0x12, 0x2D, 0x24, 0x3F, 0x36, 0x58, 0x51, 0x4A, 0x43, 0x7C, 0x75, 0x6E,
	0x67, 0x10, 0x19, 0x02, 0x0B, 0x34, 0x3D, 0x26, 0x2F, 0x73, 0x7A, 0x61,
	0x68, 0x57, 0x5E, 0x45, 0x4C, 0x3B, 0x32, 0x29, 0x20, 0x1F, 0x16, 0x0D,
	0x04, 0x6A, 0x63, 0x78, 0x71, 0x4E, 0x47, 0x5C, 0x55, 0x22, 0x2B, 0x30,
	0x39, 0x06, 0x0F, 0x14, 0x1D, 0x25, 0x2C, 0x37, 0x3E, 0x01, 0x08, 0x13,
	0x1A, 0x6D, 0x64, 0x7F, 0x76, 0x49, 0x40, 0x5B, 0x52, 0x3C, 0x35, 0x2E,
	0x27, 0x18, 0x11, 0x0A, 0x03, 0x74, 0x7D, 0x66, 0x6F, 0x50, 0x59, 0x42,
	0x4B, 0x17, 0x1E, 0x05, 0x0C, 0x33, 0x3A, 0x21, 0x28, 0x5F, 0x56, 0x4D,
	0x44, 0x7B, 0x72, 0x69, 0x60, 0x0E, 0x07, 0x1C, 0x15, 0x2A, 0x23, 0x38,
	0x31, 0x46, 0x4F, 0x54, 0x5D, 0x62, 0x6B, 0x70, 0x79};

static const unsigned short m_Crc16Table[256] = {0x0000, 0x1021, 0x2042,
	0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A, 0xB16B,
	0xC18C, 0xD1AD, 0xE1CE, 0xF1EF, 0x1231, 0x0210, 0x3273, 0x2252, 0x52B5,
	0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C,
	0xF3FF, 0xE3DE, 0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4,
	0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4, 0xB75B,
	0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC, 0x48C4, 0x58E5,
	0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823, 0xC9CC, 0xD9ED, 0xE98E,
	0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B, 0x5AF5, 0x4AD4, 0x7AB7, 0x6A96,
	0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79,
	0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03,
	0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68,
	0x9D49, 0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
	0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78, 0x9188,
	0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1,
	0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067, 0x83B9, 0x9398, 0xA3FB,
	0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1, 0x1290, 0x22F3, 0x32D2,
	0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E,
	0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447,
	0x5424, 0x4405, 0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D,
	0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB, 0x5844,
	0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3, 0xCB7D, 0xDB5C,
	0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A, 0x4A75, 0x5A54, 0x6A37,
	0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92, 0xFD2E, 0xED0F, 0xDD6C, 0xCD4D,
	0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2,
	0x2C83, 0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA,
	0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1,
	0x1EF0};

#endif

#if MBED_CONF_SD_CRC_ENABLED
static char crc7(const char* data, int length)
{
	//Calculate the CRC7 checksum for the specified data block
	char crc = 0;
	for (int i = 0; i < length; i++) {
		crc = m_Crc7Table[(crc << 1) ^ data[i]];
	}

	//Return the calculated checksum
	return crc;
}
#endif

#if MBED_CONF_SD_CRC_ENABLED
static unsigned short crc16(const char* data, int length)
{
	//Calculate the CRC16 checksum for the specified data block
	unsigned short crc = 0;
	for (int i = 0; i < length; i++) {
		crc = (crc << 8) ^ m_Crc16Table[((crc >> 8) ^ data[i]) & 0x00FF];
	}

	//Return the calculated checksum
	return crc;
}
#endif

static void sd_select(sd_t *this) {
	sd_spi_write(this, 0xFF);
	sd_spi_acquire(this);
}
static void sd_deselect(sd_t *this) {
	sd_spi_release(this);
	sd_spi_write(this, 0xFF);
}

#define SD_COMMAND_TIMEOUT 5000   /*!< Timeout in ms for response */

static int sd_cmd(sd_t *this, cmdSupported cmd, uint32_t arg, bool isAcmd, uint32_t *resp) {
	int32_t status = SD_BLOCK_DEVICE_ERROR_NONE;
	uint32_t response;

	// Select card and wait for card to be ready before sending next command
	// Note: next command will fail if card is not ready
	sd_select(this);

	// No need to wait for card to be ready when sending the stop command
	if (CMD12_STOP_TRANSMISSION != cmd) {
		if (false == sd_wait_ready(this, SD_COMMAND_TIMEOUT)) {
			DBG_PRINTF("Card not ready yet\n");
		}
	}
	// Re-try command
	for (int i = 0; i < 3; i++) {
		// Send CMD55 for APP command first
		if (isAcmd) {
			response = sd_cmd_spi(this, CMD55_APP_CMD, 0x0);
			// Wait for card to be ready after CMD55
			if (false == sd_wait_ready(this, SD_COMMAND_TIMEOUT)) {
				DBG_PRINTF("Card not ready yet\n");
			}
		}
		// Send command over SPI interface
		response = sd_cmd_spi(this, cmd, arg);
		if (R1_NO_RESPONSE == response) {
			DBG_PRINTF("No response CMD:%d\n", cmd);
			continue;
		}
		break;
	}
	// Pass the response to the command call if required
	if (NULL != resp) {
		*resp = response;
	}
	// Process the response R1  : Exit on CRC/Illegal command error/No response
	if (R1_NO_RESPONSE == response) {
		DBG_PRINTF("No response CMD:%d response: 0x%" PRIx32 "\n", cmd, response);
		sd_deselect(this);
		return SD_BLOCK_DEVICE_ERROR_NO_DEVICE;         // No device
	}
	if (response & R1_COM_CRC_ERROR) {
		DBG_PRINTF("CRC error CMD:%d response 0x%" PRIx32 "\n", cmd, response);
		sd_deselect(this);
		return SD_BLOCK_DEVICE_ERROR_CRC;                // CRC error
	}
	if (response & R1_ILLEGAL_COMMAND) {
		DBG_PRINTF("Illegal command CMD:%d response 0x%" PRIx32 "\n", cmd, response);
		if (CMD8_SEND_IF_COND == cmd) { // Illegal command is for Ver1 or not SD Card
			this->card_type = CARD_UNKNOWN;
		}
		sd_deselect(this);
		return SD_BLOCK_DEVICE_ERROR_UNSUPPORTED;      // Command not supported
	}

//	DBG_PRINTF("CMD:%d \t arg:0x%" PRIx32 " \t Response:0x%" PRIx32 "\n", cmd, arg, response);
	// Set status for other errors
	if ((response & R1_ERASE_RESET) || (response & R1_ERASE_SEQUENCE_ERROR)) {
		status = SD_BLOCK_DEVICE_ERROR_ERASE;            // Erase error
	} else if ((response & R1_ADDRESS_ERROR)
			|| (response & R1_PARAMETER_ERROR)) {
		// Misaligned address / invalid address block length
		status = SD_BLOCK_DEVICE_ERROR_PARAMETER;
	}

	// Get rest of the response part for other commands
	switch (cmd) {
	case CMD8_SEND_IF_COND:             // Response R7
		DBG_PRINTF("V2-Version Card\n");
		this->card_type = SDCARD_V2; // fallthrough
		// Note: No break here, need to read rest of the response
	case CMD58_READ_OCR:                // Response R3
		response = (sd_spi_write(this, SPI_FILL_CHAR) << 24);
		response |= (sd_spi_write(this, SPI_FILL_CHAR) << 16);
		response |= (sd_spi_write(this, SPI_FILL_CHAR) << 8);
		response |= sd_spi_write(this, SPI_FILL_CHAR);
		DBG_PRINTF("R3/R7: 0x%" PRIx32 "\n", response);
		break;

	case CMD12_STOP_TRANSMISSION:       // Response R1b
	case CMD38_ERASE:
		sd_wait_ready(this, SD_COMMAND_TIMEOUT);
		break;

	case ACMD13_SD_STATUS:             // Response R2
		response = sd_spi_write(this, SPI_FILL_CHAR);
		DBG_PRINTF("R2: 0x%" PRIx32 "\n", response);
		break;

	default:                            // Response R1
		break;
	}
	// Pass the updated response to the command
	if (NULL != resp) {
		*resp = response;
	}
	// Do not deselect card if read is in progress.
	if (((CMD9_SEND_CSD == cmd) || (ACMD22_SEND_NUM_WR_BLOCKS == cmd)
			|| (CMD24_WRITE_BLOCK == cmd) || (CMD25_WRITE_MULTIPLE_BLOCK == cmd)
			|| (CMD17_READ_SINGLE_BLOCK == cmd)
			|| (CMD18_READ_MULTIPLE_BLOCK == cmd)) && (SD_BLOCK_DEVICE_ERROR_NONE == status)) {
		return SD_BLOCK_DEVICE_ERROR_NONE;
	}
	// Deselect card
	sd_deselect(this);
	return status;
}

/* Return non-zero if the SD-card is present.
 The parameter 'pxDisk' may be null, unless device locking is necessary. */
BaseType_t FF_SDDiskDetect(FF_Disk_t *pxDisk) {

	sd_t *this = (sd_t *) pxDisk->pvTag;

	/*!< Check GPIO to detect SD */
	if (Cy_GPIO_Read(this->card_detect_gpio_port, this->card_detect_gpio_num)
			== this->card_detected_true) {
		//The socket is now occupied
		this->m_Status &= ~STA_NODISK;
		this->card_type = CARD_UNKNOWN;
		return pdTRUE;
	} else {
		//The socket is now empty
		this->m_Status |= (STA_NODISK | STA_NOINIT);
		this->card_type = SDCARD_NONE;
		DBG_PRINTF("No SD card detected!\n");
		return pdFALSE;
	}
}

#define SPI_EVENT_ERROR       (1 << 1)
#define SPI_EVENT_COMPLETE    (1 << 2)
#define SPI_EVENT_RX_OVERFLOW (1 << 3)
#define SPI_EVENT_ALL         (SPI_EVENT_ERROR | SPI_EVENT_COMPLETE | SPI_EVENT_RX_OVERFLOW)

#define SPI_FILL_WORD         (0xFFFF)
#define SPI_FILL_CHAR         (0xFF)

static bool spi_init(spi_t *this) {
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

#define SD_CMD0_GO_IDLE_STATE_RETRIES     5      /*!< Number of retries for sending CMDO */

static uint32_t sd_go_idle_state(sd_t *this) {
	uint32_t response;

	/* Reseting the MCU SPI master may not reset the on-board SDCard, in which
	 * case when MCU power-on occurs the SDCard will resume operations as
	 * though there was no reset. In this scenario the first CMD0 will
	 * not be interpreted as a command and get lost. For some cards retrying
	 * the command overcomes this situation. */
	for (int i = 0; i < SD_CMD0_GO_IDLE_STATE_RETRIES; i++) {
		sd_cmd(this, CMD0_GO_IDLE_STATE, 0x0, 0x0, &response);
		if (R1_IDLE_STATE == response) {
			break;
		}
		vTaskDelay(1);
	}
	return response;
}

/* R7 response pattern for CMD8 */
#define CMD8_PATTERN             (0xAA)

static int sd_cmd8(sd_t *this) {
	uint32_t arg = (CMD8_PATTERN << 0);         // [7:0]check pattern
	uint32_t response = 0;
	int32_t status = SD_BLOCK_DEVICE_ERROR_NONE;

	arg |= (0x1 << 8);  // 2.7-3.6V             // [11:8]supply voltage(VHS)

	status = sd_cmd(this, CMD8_SEND_IF_COND, arg, 0x0, &response);
	// Verify voltage and pattern for V2 version of card
	if ((SD_BLOCK_DEVICE_ERROR_NONE == status) && (SDCARD_V2 == this->card_type)) {
		// If check pattern is not matched, CMD8 communication is not valid
		if ((response & 0xFFF) != arg) {
			DBG_PRINTF("CMD8 Pattern mismatch 0x%" PRIx32 " : 0x%" PRIx32 "\n",
					arg, response);
			this->card_type = CARD_UNKNOWN;
			status = SD_BLOCK_DEVICE_ERROR_UNUSABLE;
		}
	}
	return status;
}

static int sd_initialise_card(sd_t *this) {
	int32_t status = SD_BLOCK_DEVICE_ERROR_NONE;
	uint32_t response, arg;

	sd_spi_go_low_frequency(this);

	// The card is transitioned from SDCard mode to SPI mode by sending the CMD0 + CS Asserted("0")
	if (sd_go_idle_state(this) != R1_IDLE_STATE) {
		DBG_PRINTF("No disk, or could not put SD card in to SPI idle state\n");
		return SD_BLOCK_DEVICE_ERROR_NO_DEVICE;
	}

	// Send CMD8, if the card rejects the command then it's probably using the
	// legacy protocol, or is a MMC, or just flat-out broken
	status = sd_cmd8(this);
	if (SD_BLOCK_DEVICE_ERROR_NONE != status && SD_BLOCK_DEVICE_ERROR_UNSUPPORTED != status) {
		return status;
	}

#if MBED_CONF_SD_CRC_ENABLED
	if (_crc_on) {
		// Enable CRC
		status = _cmd(CMD59_CRC_ON_OFF, _crc_on);
	}
#endif

	// Read OCR - CMD58 Response contains OCR register
	if (SD_BLOCK_DEVICE_ERROR_NONE
			!= (status = sd_cmd(this, CMD58_READ_OCR, 0x0, 0x0, &response))) {
		return status;
	}

	// Check if card supports voltage range: 3.3V
	if (!(response & OCR_3_3V)) {
		this->card_type = CARD_UNKNOWN;
		status = SD_BLOCK_DEVICE_ERROR_UNUSABLE;
		return status;
	}

	// HCS is set 1 for HC/XC capacity cards for ACMD41, if supported
	arg = 0x0;
	if (SDCARD_V2 == this->card_type) {
		arg |= OCR_HCS_CCS;
	}

	/* Idle state bit in the R1 response of ACMD41 is used by the card to inform the host
	 * if initialization of ACMD41 is completed. "1" indicates that the card is still initializing.
	 * "0" indicates completion of initialization. The host repeatedly issues ACMD41 until
	 * this bit is set to "0".
	 */
	TickType_t xStart = xTaskGetTickCount();
	do {
		status = sd_cmd(this, ACMD41_SD_SEND_OP_COND, arg, 1, &response);
	} while ((response & R1_IDLE_STATE)
			&& (xTaskGetTickCount() - xStart)
					< pdMS_TO_TICKS(SD_COMMAND_TIMEOUT));

	// Initialization complete: ACMD41 successful
	if ((SD_BLOCK_DEVICE_ERROR_NONE != status) || (0x00 != response)) {
		this->card_type = CARD_UNKNOWN;
		DBG_PRINTF("Timeout waiting for card\n");
		return status;
	}

	if (SDCARD_V2 == this->card_type) {
		// Get the card capacity CCS: CMD58
		if (SD_BLOCK_DEVICE_ERROR_NONE
				== (status = sd_cmd(this, CMD58_READ_OCR, 0x0, 0x0, &response))) {
			// High Capacity card
			if (response & OCR_HCS_CCS) {
				this->card_type = SDCARD_V2HC;
				DBG_PRINTF("Card Initialized: High Capacity Card\n");
			} else {
				DBG_PRINTF("%s",
						"Card Initialized: Standard Capacity Card: Version 2.x\n");
			}
		}
	} else {
		this->card_type = SDCARD_V1;
		DBG_PRINTF("Card Initialized: Version 1.x Card\n");
	}

#if MBED_CONF_SD_CRC_ENABLED
	if (!_crc_on) {
		// Disable CRC
		status = _cmd(CMD59_CRC_ON_OFF, _crc_on);
	}
#else
	status = sd_cmd(this, CMD59_CRC_ON_OFF, 0, 0, 0);
#endif

	return status;
}

static uint32_t ext_bits(unsigned char *data, int msb, int lsb) {
	uint32_t bits = 0;
	uint32_t size = 1 + msb - lsb;
	for (uint32_t i = 0; i < size; i++) {
		uint32_t position = lsb + i;
		uint32_t byte = 15 - (position >> 3);
		uint32_t bit = position & 0x7;
		uint32_t value = (data[byte] >> bit) & 1;
		bits |= value << i;
	}
	return bits;
}

static int sd_read_bytes(sd_t *this, uint8_t *buffer, uint32_t length);

static uint64_t sd_sectors(sd_t *this) {
	uint32_t c_size, c_size_mult, read_bl_len;
	uint32_t block_len, mult, blocknr;
	uint32_t hc_c_size;
	uint64_t blocks = 0, capacity = 0;

	// CMD9, Response R2 (R1 byte + 16-byte block read)
	if (sd_cmd(this, CMD9_SEND_CSD, 0x0, 0, 0) != 0x0) {
		DBG_PRINTF("Didn't get a response from the disk\n");
		return 0;
	}
	uint8_t csd[16];
	if (sd_read_bytes(this, csd, 16) != 0) {
		DBG_PRINTF("Couldn't read csd response from disk\n");
		return 0;
	}
	// csd_structure : csd[127:126]
	int csd_structure = ext_bits(csd, 127, 126);
	switch (csd_structure) {
	case 0:
		c_size = ext_bits(csd, 73, 62);            // c_size        : csd[73:62]
		c_size_mult = ext_bits(csd, 49, 47);       // c_size_mult   : csd[49:47]
		read_bl_len = ext_bits(csd, 83, 80); // read_bl_len   : csd[83:80] - the *maximum* read block length
		block_len = 1 << read_bl_len;               // BLOCK_LEN = 2^READ_BL_LEN
		mult = 1 << (c_size_mult + 2); // MULT = 2^C_SIZE_MULT+2 (C_SIZE_MULT < 8)
		blocknr = (c_size + 1) * mult;            // BLOCKNR = (C_SIZE+1) * MULT
		capacity = (uint64_t) blocknr * block_len; // memory capacity = BLOCKNR * BLOCK_LEN
		blocks = capacity / _block_size;
		DBG_PRINTF("Standard Capacity: c_size: %" PRIu32 "\n", c_size);
		DBG_PRINTF("Sectors: 0x%" PRIx64 " : %" PRIu64 "\n", blocks, blocks);
		DBG_PRINTF("Capacity: 0x%" PRIx64 " : %" PRIu64 " MB\n", capacity,
				(capacity / (1024U * 1024U)));
		break;

	case 1:
		hc_c_size = ext_bits(csd, 69, 48);     // device size : C_SIZE : [69:48]
		blocks = (hc_c_size + 1) << 10; // block count = C_SIZE+1) * 1K byte (512B is block size)
		DBG_PRINTF("SDHC/SDXC Card: hc_c_size: %" PRIu32 "\n", hc_c_size);
		uint32_t b = blocks;
		DBG_PRINTF("Sectors: %8lu\n", b);
		DBG_PRINTF("Capacity: %8lu MB\n", (b / (2048U)));
		break;

	default:
		DBG_PRINTF("CSD struct unsupported\n");
        configASSERT(!"CSD struct unsupported\n");
		return 0;
	};
	return blocks;
}

uint64_t FF_SDDiskSectors(FF_Disk_t *pxDisk) {
	sd_t *this = (sd_t *) pxDisk->pvTag;
	return sd_sectors(this);
}

// An SD card can only do one thing at a time
static void sd_lock(sd_t *this) {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
}
static void sd_unlock(sd_t *this) {
	xSemaphoreGiveRecursive(this->mutex);
}

static int sd_driver_init(FF_Disk_t *pxDisk) {
//	STA_NOINIT = 0x01, /* Drive not initialized */
//	STA_NODISK = 0x02, /* No medium in the drive */
//	STA_PROTECT = 0x04 /* Write protected */

	sd_t *this = (sd_t *) pxDisk->pvTag;

	// bool __atomic_test_and_set (void *ptr, int memorder)
	// This built-in function performs an atomic test-and-set operation on the byte at *ptr. 
	// The byte is set to some implementation defined nonzero “set” value 
	// and the return value is true if and only if the previous contents were “set”. 
	if (__atomic_test_and_set(&(this->initialized), __ATOMIC_SEQ_CST))
		return this->m_Status;

	this->ready = xEventGroupCreate();

	//Initialize the member variables
	this->card_type = SDCARD_NONE;
//    m_Crc = true;
//    m_LargeFrames = false;
//    m_WriteValidation = true;
	this->m_Status = STA_NOINIT;

	//Make sure there's a card in the socket before proceeding
	FF_SDDiskDetect(pxDisk);
	if (this->m_Status & STA_NODISK) {
		sd_unlock(this);
		return this->m_Status;
	}
	//Make sure we're not already initialized before proceeding
	if (!(this->m_Status & STA_NOINIT)) {
		sd_unlock(this);
		return this->m_Status;
	}
	if (!spi_init(this->spi)) {
		sd_unlock(this);
		return this->m_Status;
	}
	int err = SD_BLOCK_DEVICE_ERROR_NONE;
	err = sd_initialise_card(this);
	if (!(err == SD_BLOCK_DEVICE_ERROR_NONE)) {
		DBG_PRINTF("Fail to initialize card\n");
		sd_unlock(this);
		return this->m_Status;
	}
	DBG_PRINTF("SD card initialized\n");
	this->sectors = sd_sectors(this);
	if (0 == this->sectors) {
        // CMD9 failed
		sd_unlock(this);
		return this->m_Status;
	}
	// Set block length to 512 (CMD16)
	if (sd_cmd(this, CMD16_SET_BLOCKLEN, _block_size, 0, 0) != 0) {
		DBG_PRINTF("Set %" PRIu32 "-byte block timed out\n", _block_size);
		sd_unlock(this);
		return this->m_Status;
	}
	// Set SCK for data transfer
	sd_spi_go_high_frequency(this);

	//The card is now initialized
	this->m_Status &= ~STA_NOINIT;
	sd_unlock(this);

	// Notify waiters
	xEventGroupSetBits(this->ready, 1);

	//Return the disk status
	return this->m_Status;
}

// SPI function to wait till chip is ready and sends start token
static bool sd_wait_token(sd_t *this, uint8_t token) {    
	TickType_t xStart = xTaskGetTickCount();
	do {
		if (token == sd_spi_write(this, SPI_FILL_CHAR)) {
			return true;
		}
	} while ((xTaskGetTickCount() - xStart) < pdMS_TO_TICKS(300)); // Wait for 300 msec for start token
	DBG_PRINTF("sd_wait_token: timeout\n");
	return false;
}

#define SPI_START_BLOCK          (0xFE)      /*!< For Single Block Read/Write and Multiple Block Read */

static int sd_read_bytes(sd_t *this, uint8_t *buffer, uint32_t length) {
	uint16_t crc;

	// read until start byte (0xFE)
	if (false == sd_wait_token(this, SPI_START_BLOCK)) {
		DBG_PRINTF("Read timeout\n");
		sd_deselect(this);
		return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
	}
	// read data
	for (uint32_t i = 0; i < length; i++) {
		buffer[i] = sd_spi_write(this, SPI_FILL_CHAR);
	}
	// Read the CRC16 checksum for the data block
	crc = (sd_spi_write(this, SPI_FILL_CHAR) << 8);
	crc |= sd_spi_write(this, SPI_FILL_CHAR);

#if MBED_CONF_SD_CRC_ENABLED
	if (_crc_on) {
		uint32_t crc_result;
		// Compute and verify checksum
		_crc16.compute((void *)buffer, length, &crc_result);
		if ((uint16_t)crc_result != crc) {
			debug_if(SD_DBG, "_read_bytes: Invalid CRC received 0x%" PRIx16 " result of computation 0x%" PRIx16 "\n",
					crc, (uint16_t)crc_result);
			_deselect();
			return SD_BLOCK_DEVICE_ERROR_CRC;
		}
	}
#endif

	sd_deselect(this);
	return 0;
}
static int sd_read_block(sd_t *this, uint8_t *buffer, uint32_t length) {
	uint16_t crc;

	// read until start byte (0xFE)
	if (false == sd_wait_token(this, SPI_START_BLOCK)) {
		DBG_PRINTF("Read timeout\n");
		return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
	}
	// read data
	// bool spi_transfer(const uint8_t *tx, uint8_t *rx, size_t length)
	if (!sd_spi_transfer(this, NULL, buffer, length)) {
		return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
	}
	// Read the CRC16 checksum for the data block
	crc = (sd_spi_write(this, SPI_FILL_CHAR) << 8);
	crc |= sd_spi_write(this, SPI_FILL_CHAR);

#if MBED_CONF_SD_CRC_ENABLED
	if (_crc_on) {
		uint32_t crc_result;
		// Compute and verify checksum
		_crc16.compute((void *)buffer, length, &crc_result);
		if ((uint16_t)crc_result != crc) {
			debug_if(SD_DBG, "_read_bytes: Invalid CRC received 0x%" PRIx16 " result of computation 0x%" PRIx16 "\n",
					crc, (uint16_t)crc_result);
			return SD_BLOCK_DEVICE_ERROR_CRC;
		}
	}
#endif

	return SD_BLOCK_DEVICE_ERROR_NONE;
}

static int 
sd_read_blocks(sd_t *this, uint8_t *buffer, uint64_t ulSectorNumber, uint32_t ulSectorCount) {

	uint32_t blockCnt = ulSectorCount;

	if (ulSectorNumber + blockCnt > this->sectors) {
		return SD_BLOCK_DEVICE_ERROR_PARAMETER;
	}
    if (!this->initialized) {
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }
	if (this->m_Status & STA_NOINIT)
		return SD_BLOCK_DEVICE_ERROR_PARAMETER;

	int status = SD_BLOCK_DEVICE_ERROR_NONE;

	uint64_t addr;
	// SDSC Card (CCS=0) uses byte unit address
	// SDHC and SDXC Cards (CCS=1) use block unit address (512 Bytes unit)
	if (SDCARD_V2HC == this->card_type) {
		addr = ulSectorNumber;
	} else {
		addr = ulSectorNumber * _block_size;
	}
	// Write command ro receive data
	if (blockCnt > 1) {
		status = sd_cmd(this, CMD18_READ_MULTIPLE_BLOCK, addr, 0, 0);
	} else {
		status = sd_cmd(this, CMD17_READ_SINGLE_BLOCK, addr, 0, 0);
	}
	if (SD_BLOCK_DEVICE_ERROR_NONE != status) {
		return status;
	}
	// receive the data : one block at a time
	while (blockCnt) {
		if (0 != sd_read_block(this, buffer, _block_size)) {
			status = SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
			break;
		}
		buffer += _block_size;
		--blockCnt;
	}
	sd_deselect(this);

	// Send CMD12(0x00000000) to stop the transmission for multi-block transfer
	if (ulSectorCount > 1) {
		status = sd_cmd(this, CMD12_STOP_TRANSMISSION, 0x0, 0, 0);
	}
	return status;
}

int32_t prvFFRead(uint8_t *pucDestination, /* Destination for data being read. */
        uint32_t ulSectorNumber, /* Sector from which to start reading data. */
        uint32_t ulSectorCount, /* Number of sectors to read. */
        FF_Disk_t *pxDisk) /* Describes the disk being read from. */
{
	sd_t *this = (sd_t *) pxDisk->pvTag;
	sd_lock(this);
	int status = sd_read_blocks((sd_t *) pxDisk->pvTag, pucDestination,
			ulSectorNumber, ulSectorCount);
	sd_unlock(this);
	if (SD_BLOCK_DEVICE_ERROR_NONE == status) {
		return FF_ERR_NONE;
	} else {
		return FF_ERR_DEVICE_DRIVER_FAILED;
	}
}

static bool sd_spi_write_block(sd_t *this, const uint8_t *tx_buffer, size_t length) {

	// bool spi_transfer(const uint8_t *tx, uint8_t *rx, size_t length)
	bool ret = sd_spi_transfer(this, tx_buffer, NULL, length);

	return ret;
}

static uint8_t sd_write_block(sd_t *this, const uint8_t *buffer, uint8_t token, uint32_t length) {

	uint32_t crc = (~0);
	uint8_t response = 0xFF;

	// indicate start of block
	sd_spi_write(this, token);

	// write the data
	bool rc = sd_spi_write_block(this, buffer, length);
	configASSERT(rc);

#if MBED_CONF_SD_CRC_ENABLED
	if (_crc_on) {
		// Compute CRC
		_crc16.compute((void *)buffer, length, &crc);
	}
#endif

	// write the checksum CRC16
	sd_spi_write(this, crc >> 8);
	sd_spi_write(this, crc);

	// check the response token
	response = sd_spi_write(this, SPI_FILL_CHAR);

	// Wait for last block to be written
	if (false == sd_wait_ready(this, SD_COMMAND_TIMEOUT)) {
		DBG_PRINTF("Card not ready yet \n");
	}
	return (response & SPI_DATA_RESPONSE_MASK);
}

static int sd_reserve(sd_t *this, uint32_t blockCnt) {
    return sd_cmd(this, ACMD23_SET_WR_BLK_ERASE_COUNT, blockCnt, 1, 0);
}
int FFReserve(FF_FILE *pxFile, uint32_t blockCnt) {
    configASSERT(pxFile->pxIOManager->xBlkDevice.pxDisk->pvTag);
	sd_t *this = (sd_t *) pxFile->pxIOManager->xBlkDevice.pxDisk->pvTag;   
    return sd_reserve(this, blockCnt);
}

/** Program blocks to a block device
 *
 *
 *  @param buffer       Buffer of data to write to blocks
 *  @param ulSectorNumber     Logical Address of block to begin writing to (LBA)
 *  @param blockCnt     Size to write in blocks
 *  @return         SD_BLOCK_DEVICE_ERROR_NONE(0) - success
 *                  SD_BLOCK_DEVICE_ERROR_NO_DEVICE - device (SD card) is missing or not connected
 *                  SD_BLOCK_DEVICE_ERROR_CRC - crc error
 *                  SD_BLOCK_DEVICE_ERROR_PARAMETER - invalid parameter
 *                  SD_BLOCK_DEVICE_ERROR_UNSUPPORTED - unsupported command
 *                  SD_BLOCK_DEVICE_ERROR_NO_INIT - device is not initialized
 *                  SD_BLOCK_DEVICE_ERROR_WRITE - SPI write error
 *                  SD_BLOCK_DEVICE_ERROR_ERASE - erase error
 */
static int 
sd_write_blocks(sd_t *this, const uint8_t *buffer, uint64_t ulSectorNumber, uint32_t blockCnt) {
	if (ulSectorNumber + blockCnt > this->sectors) {
		return SD_BLOCK_DEVICE_ERROR_PARAMETER;
	}
    if (!this->initialized) {
        return SD_BLOCK_DEVICE_ERROR_NO_INIT;
    }
	if (this->m_Status & STA_NOINIT)
		return SD_BLOCK_DEVICE_ERROR_PARAMETER;

	int status = SD_BLOCK_DEVICE_ERROR_NONE;
	uint8_t response;
	uint64_t addr;

	// SDSC Card (CCS=0) uses byte unit address
	// SDHC and SDXC Cards (CCS=1) use block unit address (512 Bytes unit)
	if (SDCARD_V2HC == this->card_type) {
		addr = ulSectorNumber;
	} else {
		addr = ulSectorNumber * _block_size;
	}
	// Send command to perform write operation
	if (blockCnt == 1) {
		// Single block write command
		if (SD_BLOCK_DEVICE_ERROR_NONE
				!= (status = sd_cmd(this, CMD24_WRITE_BLOCK, addr, 0, 0))) {
			return status;
		}
		// Write data
		response = sd_write_block(this, buffer, SPI_START_BLOCK, _block_size);

		// Only CRC and general write error are communicated via response token
		if (response != SPI_DATA_ACCEPTED) {
			DBG_PRINTF("Single Block Write failed: 0x%x \n", response);
			status = SD_BLOCK_DEVICE_ERROR_WRITE;
		}
	} else {
		// Pre-erase setting prior to multiple block write operation
		sd_cmd(this, ACMD23_SET_WR_BLK_ERASE_COUNT, blockCnt, 1, 0);

		// Multiple block write command
		if (SD_BLOCK_DEVICE_ERROR_NONE
				!= (status = sd_cmd(this, CMD25_WRITE_MULTIPLE_BLOCK, addr, 0, 0))) {
			return status;
		}
		// Write the data: one block at a time
		do {
			response = sd_write_block(this, buffer, SPI_START_BLK_MUL_WRITE, _block_size);
			if (response != SPI_DATA_ACCEPTED) {
				DBG_PRINTF("Multiple Block Write failed: 0x%x\n", response);
				break;
			}
			buffer += _block_size;
		} while (--blockCnt);     // Receive all blocks of data

		/* In a Multiple Block write operation, the stop transmission will be done by
		 * sending 'Stop Tran' token instead of 'Start Block' token at the beginning
		 * of the next block
		 */
		sd_spi_write(this, SPI_STOP_TRAN);
	}
	sd_deselect(this);
	return status;
}

int32_t FFWrite(uint8_t *pucSource, /* Source of data to be written. */
        uint32_t ulSectorNumber, /* The first sector being written to. */
        uint32_t ulSectorCount, /* The number of sectors to write. */
        FF_Disk_t *pxDisk) /* Describes the disk being written to. */
{
	sd_t *this = (sd_t *) pxDisk->pvTag;

    sd_lock(this);
	int status = sd_write_blocks((sd_t *) pxDisk->pvTag, pucSource,
			ulSectorNumber, ulSectorCount);
	sd_unlock(this);

	if (SD_BLOCK_DEVICE_ERROR_NONE == status) {
		return FF_ERR_NONE;
	} else {
		DBG_PRINTF("sd_write_blocks returned %d\n", status);
		return FF_ERR_DEVICE_DRIVER_FAILED;
	}
}

FF_Disk_t *FF_SDDiskInit(const char *pcName) {
	FF_Error_t xError = 0;
	FF_Disk_t *pxDisk = NULL;
	FF_CreationParameters_t xParameters;

	const BaseType_t SECTOR_SIZE = 512;
	//"Cluster size" is 32 kilobytes on one 8GB card
	const uint32_t xIOManagerCacheSize = 4 * SECTOR_SIZE;
//	const uint32_t xIOManagerCacheSize = 40 * SECTOR_SIZE; // 20,006 bytes per Type 2 record

	/* Check the validity of the xIOManagerCacheSize parameter. */
	configASSERT((xIOManagerCacheSize % SECTOR_SIZE) == 0);
	configASSERT((xIOManagerCacheSize >= (2 * SECTOR_SIZE)));

	/* Attempt to allocated the FF_Disk_t structure. */
	pxDisk = (FF_Disk_t *) pvPortMalloc(sizeof(FF_Disk_t));

	if (pxDisk != NULL) {

		/* It is advisable to clear the entire structure to zero after it has been
		 allocated – that way the media driver will be compatible with future
		 FreeRTOS+FAT versions, in which the FF_Disk_t structure may include
		 additional members. */
		memset(pxDisk, 0, sizeof(FF_Disk_t));

		size_t i;
		for (i = 0; i < sizeof(sd_cards) / sizeof(sd_cards[0]); ++i) {
			if (0 == strcmp(sd_cards[i].pcName, pcName))
				break;
		}
		if (sizeof(sd_cards) / sizeof(sd_cards[0]) == i) {
			DBG_PRINTF("FF_SDDiskInit: unknown name %s\n", pcName);
			configASSERT(!"FF_SDDiskInit failed!");
			return NULL;
		}

		sd_t *this = &sd_cards[i];

		/* The pvTag member of the FF_Disk_t structure allows the structure to be
		 extended to also include media specific parameters.  */
		pxDisk->pvTag = this;

		this->mutex = xSemaphoreCreateRecursiveMutex();
		sd_lock(this);

		// Initialize the media driver
		if (sd_driver_init(pxDisk)) {
			// Couldn't init
			vPortFree(pxDisk);
			sd_unlock(this);
			return NULL;
		}

		/* The pvTag member of the FF_Disk_t structure allows the structure to be
		 extended to also include media specific parameters.  The only media
		 specific data that needs to be stored in the FF_Disk_t structure for a
		 RAM disk is the location of the RAM buffer itself – so this is stored
		 directly in the FF_Disk_t’s pvTag member. */
//        pxDisk->pvTag = ( void * ) pucDataBuffer;
		/* The signature is used by the disk read and disk write functions to
		 ensure the disk being accessed is a RAM disk. */
//        pxDisk->ulSignature = ramSIGNATURE;
		/* The number of sectors is recorded for bounds checking in the read and
		 write functions. */
		pxDisk->ulNumberOfSectors = sd_cards[i].sectors;

		/* Create the IO manager that will be used to control the RAM disk –
		 the FF_CreationParameters_t structure completed with the required
		 parameters, then passed into the FF_CreateIOManager() function. */
		memset(&xParameters, 0, sizeof xParameters);
		xParameters.pucCacheMemory = NULL;
		xParameters.ulMemorySize = xIOManagerCacheSize;
		xParameters.ulSectorSize = SECTOR_SIZE;
		xParameters.fnWriteBlocks = FFWrite;
		xParameters.fnReadBlocks = prvFFRead;
		xParameters.pxDisk = pxDisk;

		xParameters.pvSemaphore = (void *) xSemaphoreCreateRecursiveMutex();
		xParameters.xBlockDeviceIsReentrant = pdTRUE;

		pxDisk->pxIOManager = FF_CreateIOManger(&xParameters, &xError);

		if ((pxDisk->pxIOManager != NULL) && ( FF_isERR( xError ) == pdFALSE)) {
			/* Record that the disk has been initialised. */
			pxDisk->xStatus.bIsInitialised = pdTRUE;
		} else {
			/* The disk structure was allocated, but the disk’s IO manager could
			 not be allocated, so free the disk again. */
            FF_SDDiskDelete( pxDisk );
			pxDisk = NULL;
			DBG_PRINTF("FF_RAMDiskInit: FF_CreateIOManger: %s\n",
					(const char *) FF_GetErrMessage(xError));
			configASSERT(!"disk's IO manager could not be allocated!");
		}
		sd_unlock(this);
	}
	return pxDisk;
}

#if 0        
BaseType_t FF_SDDiskReinit(FF_Disk_t *pxDisk) {
	(void) pxDisk;
	return pdFALSE;
}
#endif

/* Unmount the volume */
void FF_SDDiskUnmount(FF_Disk_t *pxDisk, const char *pcPath) {
	sd_t *this = (sd_t *) pxDisk->pvTag;
	sd_lock(this);

	FF_FS_Remove(pcPath);
	FF_PRINTF("FF_FS_Remove: %s\n", (const char *) FF_GetErrMessage(xError));

	/*Unmount the partition. */
	/* FF_Error_t xError = */FF_Unmount(pxDisk);
	FF_PRINTF("FF_Unmount: %s\n", (const char *) FF_GetErrMessage(xError));
	pxDisk->xStatus.bIsMounted = pdFALSE;

	FF_SDDiskDelete(pxDisk);
	pxDisk = 0;
	sd_unlock(this);
}

/* Mount the volume */
BaseType_t 
FF_SDDiskMount(FF_Disk_t **ppxDisk, const char *pcDevName, const char *pcPath) {
	FF_PRINTF("Mounting %s at %s\n", pcDevName, pcPath);
	// FF_Disk_t *FF_SDDiskInit( const char *pcName );
	*ppxDisk = FF_SDDiskInit(pcDevName);
	configASSERT(*ppxDisk);

	sd_t *this = (sd_t *) (*ppxDisk)->pvTag;
	sd_lock(this);

	/* Record the partition number the FF_Disk_t structure is, then
	 mount the partition. */
	(*ppxDisk)->xStatus.bPartitionNumber = 0;

	/* Mount the partition. */
	FF_Error_t xError = FF_Mount(*ppxDisk,
			(*ppxDisk)->xStatus.bPartitionNumber);
	FF_PRINTF("FF_Mount: %s\n", (const char *) FF_GetErrMessage(xError));
	configASSERT(FF_isERR( xError ) == pdFALSE);
	(*ppxDisk)->xStatus.bIsMounted = pdTRUE;

	/* The partition mounted successfully, add it to the virtual
	 file system – where it will appear as a directory off the file
	 system’s root directory. */
	/*
	 * Add a file system
	 * The path must be absolute, e.g. start with a slash
	 * The second argument is the FF_Disk_t structure that is handling the driver
	 */
	int iReturn = FF_FS_Add(pcPath, *ppxDisk);
	FF_PRINTF("FF_FS_Add: %s\n", (const char *) FF_GetErrMessage(xError));
	configASSERT(iReturn);

    //FIXME
	/* Block to wait for event bits to become set within the event group. */
//      EventBits_t xEventGroupWaitBits(
//                       const EventGroupHandle_t xEventGroup,
//                       const EventBits_t uxBitsToWaitFor,
//                       const BaseType_t xClearOnExit,
//                       const BaseType_t xWaitForAllBits,
//                       TickType_t xTicksToWait );    
	EventBits_t xEventGroupValue = xEventGroupWaitBits(this->ready, 1, pdFALSE,
			pdTRUE, pdMS_TO_TICKS(1000));
	if (!xEventGroupValue) {
		FF_PRINTF("Device %s NOT READY\n", pcDevName);
		configASSERT(xEventGroupValue);
	}
	sd_unlock(this);
	return iReturn;
}

BaseType_t FF_SDDiskDelete(FF_Disk_t *pxDisk) {
	if (pxDisk != NULL) {
		pxDisk->ulSignature = 0;
		pxDisk->xStatus.bIsInitialised = 0;
		if (pxDisk->pxIOManager != NULL) {
			FF_DeleteIOManager(pxDisk->pxIOManager);
		}
		vPortFree(pxDisk);
	}
	return pdPASS;
}

#define HUNDRED_64_BIT			100ULL
#define SECTOR_SIZE				512UL
#define PARTITION_NUMBER		0 /* Only a single partition is used. */
#define BYTES_PER_KB			( 1024ull )
#define SECTORS_PER_KB			( BYTES_PER_KB / 512ull )

/* Show some partition information */
BaseType_t FF_SDDiskShowPartition(FF_Disk_t *pxDisk) {
	FF_Error_t xError;
	uint64_t ullFreeSectors;
	uint32_t ulTotalSizeKB, ulFreeSizeKB;
	int iPercentageFree;
	FF_IOManager_t *pxIOManager;
	const char *pcTypeName = "unknown type";
	BaseType_t xReturn = pdPASS;

	if (pxDisk == NULL) {
		xReturn = pdFAIL;
	} else {
		pxIOManager = pxDisk->pxIOManager;

		DBG_PRINTF("Reading FAT and calculating Free Space\n");

		switch (pxIOManager->xPartition.ucType) {
		case FF_T_FAT12:
			pcTypeName = "FAT12";
			break;

		case FF_T_FAT16:
			pcTypeName = "FAT16";
			break;

		case FF_T_FAT32:
			pcTypeName = "FAT32";
			break;

		default:
			pcTypeName = "UNKOWN";
			break;
		}

		FF_GetFreeSize(pxIOManager, &xError);

		ullFreeSectors = pxIOManager->xPartition.ulFreeClusterCount
				* pxIOManager->xPartition.ulSectorsPerCluster;
		if (pxIOManager->xPartition.ulDataSectors == (uint32_t) 0) {
			iPercentageFree = 0;
		} else {
			iPercentageFree = (int) (( HUNDRED_64_BIT * ullFreeSectors
					+ pxIOManager->xPartition.ulDataSectors / 2)
					/ ((uint64_t) pxIOManager->xPartition.ulDataSectors));
		}

		ulTotalSizeKB = pxIOManager->xPartition.ulDataSectors / SECTORS_PER_KB;
		ulFreeSizeKB = (uint32_t)(ullFreeSectors / SECTORS_PER_KB);

		/* It is better not to use the 64-bit format such as %Lu because it
		 might not be implemented. */
		DBG_PRINTF("Partition Nr   %8u\n", pxDisk->xStatus.bPartitionNumber);
		DBG_PRINTF("Type           %8u (%s)\n", pxIOManager->xPartition.ucType,
				pcTypeName);
		DBG_PRINTF("VolLabel       '%8s' \n",
				pxIOManager->xPartition.pcVolumeLabel);
		DBG_PRINTF("TotalSectors   %8lu\n",
				pxIOManager->xPartition.ulTotalSectors);
		DBG_PRINTF("SecsPerCluster %8lu\n",
				pxIOManager->xPartition.ulSectorsPerCluster);
		DBG_PRINTF("Size           %8lu KB\n", ulTotalSizeKB);
		DBG_PRINTF("FreeSize       %8lu KB ( %d perc free )\n", ulFreeSizeKB,
				iPercentageFree);
	}

	return xReturn;
}

uint32_t get_block_size() {
	return _block_size;
}

/*-----------------------------------------------------------*/
