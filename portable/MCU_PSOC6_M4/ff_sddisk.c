/*
 * FreeRTOS+FAT DOS Compatible Embedded FAT File System 
 *     https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_FAT/index.html
 * ported to Cypress CY8C6347BZI-BLD53.
 *
 * Editor: Carl Kugler (carlk3@gmail.com)
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

#include "ff_sddisk.h"
#include "ff_ioman.h"
#include "sd_card.h"

/* A function to write sectors to the device. */
static int32_t write(uint8_t *pucSource, /* Source of data to be written. */
		uint32_t ulSectorNumber, /* The first sector being written to. */
		uint32_t ulSectorCount, /* The number of sectors to write. */
		FF_Disk_t *pxDisk) /* Describes the disk being written to. */
{
	int status = sd_write_blocks(pxDisk->pvTag, pucSource, ulSectorNumber, ulSectorCount);
	if (SD_BLOCK_DEVICE_ERROR_NONE == status) {
		return FF_ERR_NONE;
	} else {
		return FF_ERR_DEVICE_DRIVER_FAILED;
	}
}

static int32_t read(uint8_t *pucDestination, /* Destination for data being read. */
		uint32_t ulSectorNumber, /* Sector from which to start reading data. */
		uint32_t ulSectorCount, /* Number of sectors to read. */
		FF_Disk_t *pxDisk) /* Describes the disk being read from. */
{
	int status = sd_read_blocks(pxDisk->pvTag, pucDestination, ulSectorNumber, ulSectorCount);
	if (SD_BLOCK_DEVICE_ERROR_NONE == status) {
		return FF_ERR_NONE;
	} else {
		return FF_ERR_DEVICE_DRIVER_FAILED;
	}
}

BaseType_t FF_SDDiskDetect( FF_Disk_t *pxDisk ) {
    if (!pxDisk)
        return false;
    return sd_card_detect(pxDisk->pvTag);
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

        sd_t *sd =sd_get_by_name(pcName);
        if (!sd) {
            FF_PRINTF("FF_SDDiskInit: unknown name %s\n", pcName);
			return NULL;            
        }
		/* The pvTag member of the FF_Disk_t structure allows the structure to be
		 extended to also include media specific parameters.  */
		pxDisk->pvTag = sd;

		// Initialize the media driver
		if (sd_driver_init(sd)) {
			// Couldn't init
			vPortFree(pxDisk);
			return NULL;
		}

		/* The number of sectors is recorded for bounds checking in the read and
		 write functions. */
		pxDisk->ulNumberOfSectors = sd->sectors;

		/* Create the IO manager that will be used to control the disk –
		 the FF_CreationParameters_t structure completed with the required
		 parameters, then passed into the FF_CreateIOManager() function. */
		memset(&xParameters, 0, sizeof xParameters);
		xParameters.pucCacheMemory = NULL;
		xParameters.ulMemorySize = xIOManagerCacheSize;
		xParameters.ulSectorSize = SECTOR_SIZE;
		xParameters.fnWriteBlocks = write;
		xParameters.fnReadBlocks = read;
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
			FF_PRINTF("FF_SDDiskInit: FF_CreateIOManger: %s\n",
					(const char *) FF_GetErrMessage(xError));
			configASSERT(!"disk's IO manager could not be allocated!");
		}
	}
	return pxDisk;
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

		FF_PRINTF("Reading FAT and calculating Free Space\n");

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
		FF_PRINTF("Partition Nr   %8u\n", pxDisk->xStatus.bPartitionNumber);
		FF_PRINTF("Type           %8u (%s)\n", pxIOManager->xPartition.ucType,
				pcTypeName);
		FF_PRINTF("VolLabel       '%8s' \n",
				pxIOManager->xPartition.pcVolumeLabel);
		FF_PRINTF("TotalSectors   %8lu\n",
				pxIOManager->xPartition.ulTotalSectors);
		FF_PRINTF("SecsPerCluster %8lu\n",
				pxIOManager->xPartition.ulSectorsPerCluster);
		FF_PRINTF("Size           %8lu KB\n", ulTotalSizeKB);
		FF_PRINTF("FreeSize       %8lu KB ( %d perc free )\n", ulFreeSizeKB,
				iPercentageFree);
	}

	return xReturn;
}

/*-----------------------------------------------------------*/
