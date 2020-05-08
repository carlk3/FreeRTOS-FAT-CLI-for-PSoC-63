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

#include "ff_headers.h"
#include "ff_sddisk.h"
#include "ff_stdio.h"
#include "hw_config.h"

#include "ff_utils.h"

static FF_Error_t prvPartitionAndFormatDisk( FF_Disk_t *pxDisk )
{
FF_PartitionParameters_t xPartition;
FF_Error_t xError;

	/* Media cannot be used until it has been partitioned.  In this
	case a single partition is to be created that fills all available space – so
	by clearing the xPartition structure to zero. */
	memset( &xPartition, 0x00, sizeof( xPartition ) );
	xPartition.ulSectorCount = pxDisk->ulNumberOfSectors;
    
    switch (xPartition.ulSectorCount) {
        // For alignment:
        // See https://www.partitionwizard.com/help/align-partition.html
        case 15519744: // 8GB (7.4 GiB) SD card
            xPartition.ulHiddenSectors = 2048; 
            break;
        case 31205376: // 16GB (14.9 GiB) SD card
            xPartition.ulHiddenSectors = 67584; 
            break;
        case 62325760: // 32GB (29.72 GiB) SD card
            xPartition.ulHiddenSectors = 8192; 
            break;
        case 124702720: // 64GB (59.46 GiB) SD card
            xPartition.ulHiddenSectors = 32768; 
            break;
        case 249733120: // 128GB (119.08 GiB) SD card
            xPartition.ulHiddenSectors = 2048; 
            break;               
    }        
//	xPartition.xPrimaryCount = PRIMARY_PARTITIONS;
//	xPartition.eSizeType = eSizeIsQuota;

	/* Perform the partitioning. */
	xError = FF_Partition( pxDisk, &xPartition );

	/* Print out the result of the partition operation. */
	FF_PRINTF( "FF_Partition: %s\n", FF_GetErrMessage( xError ) );

	/* Was the disk partitioned successfully? */
	if( FF_isERR( xError ) == pdFALSE )
	{
		/* The disk was partitioned successfully.  Format the first partition. */
		xError = FF_Format( pxDisk, 0, pdFALSE, pdFALSE );

		/* Print out the result of the format operation. */
		FF_PRINTF( "FF_Format: %s\n", FF_GetErrMessage( xError ) );
	}

	return xError;
}

bool format(FF_Disk_t **ppxDisk, const char *const devName) {
	*ppxDisk = FF_SDDiskInit( devName );
	if (!*ppxDisk) {
		return false;
	}
	FF_Error_t e = prvPartitionAndFormatDisk(*ppxDisk);
	return FF_ERR_NONE == e ? true : false;
}

bool mount(FF_Disk_t **ppxDisk, const char *const devName, const char *const path) {
    configASSERT(ppxDisk);
	if (!(*ppxDisk)
		|| !(*ppxDisk)->xStatus.bIsInitialised)
	{
    	*ppxDisk = FF_SDDiskInit( devName );
    }
	if (!*ppxDisk) {
		return false;
	}    
	if (!(*ppxDisk)->xStatus.bIsMounted) {
    	// BaseType_t FF_SDDiskMount( FF_Disk_t *pDisk );
    	FF_Error_t xError = FF_SDDiskMount(*ppxDisk);
    	if (FF_isERR( xError ) != pdFALSE) {
    		FF_PRINTF("FF_SDDiskMount: %s\n", (const char *) FF_GetErrMessage(xError));
    		return false;
    	}
    }
	return FF_FS_Add( path, *ppxDisk );
}
void unmount(FF_Disk_t *pxDisk, const char *pcPath) {
	FF_FS_Remove(pcPath);

	/*Unmount the partition. */
	FF_Error_t xError = FF_SDDiskUnmount(pxDisk);
	if (FF_isERR( xError ) != pdFALSE) {
		FF_PRINTF("FF_Unmount: %s\n", (const char *) FF_GetErrMessage(xError));
	}
	// FF_SDDiskDelete(pxDisk);    
}

void eject(const char *const name) {
	sd_card_t *pSD = sd_get_by_name(name);
	if (!pSD) {
        FF_PRINTF("Unknown device name %s\n", name);
        return;
    }
    size_t i;
    for (i = 0; i < pSD->ff_disk_count; ++i) {
    	FF_Disk_t *pxDisk = pSD->ff_disks[i];
    	if (pxDisk) {
    		if (pxDisk->xStatus.bIsMounted) {
    			FF_FlushCache(pxDisk->pxIOManager);
    			FF_PRINTF("Invalidating %s\n", pSD->pcName);     
    			FF_Invalidate(pxDisk->pxIOManager);                                
    			FF_PRINTF("Unmounting %s\n", pSD->pcName);                                     
    			FF_Unmount(pxDisk);
    			pxDisk->xStatus.bIsMounted = pdFALSE;    
    			Cy_GPIO_Write(BlueLED_PORT, BlueLED_NUM, 1) ;     
    		}       
            FF_SDDiskDelete(pxDisk);
        }
    }
	sd_deinit(pSD);    
	Cy_GPIO_Write(RedLED_PORT, RedLED_NUM, 1) ;         
}

void getFree(FF_Disk_t *pxDisk, uint64_t *pFreeMB, unsigned *pFreePct) {
	FF_Error_t xError;
	uint64_t ullFreeSectors, ulFreeSizeKB;
	int iPercentageFree;

	configASSERT(pxDisk);
	FF_IOManager_t *pxIOManager = pxDisk->pxIOManager;
    
	FF_GetFreeSize(pxIOManager, &xError);

	ullFreeSectors = pxIOManager->xPartition.ulFreeClusterCount * pxIOManager->xPartition.ulSectorsPerCluster;
	if (pxIOManager->xPartition.ulDataSectors == 0) {
		iPercentageFree = 0;
	} else {
		iPercentageFree = (int) (( 100ULL * ullFreeSectors + pxIOManager->xPartition.ulDataSectors / 2)
				/ ((uint64_t) pxIOManager->xPartition.ulDataSectors));
	}

    const int SECTORS_PER_KB = 2;
	ulFreeSizeKB = (uint32_t)(ullFreeSectors / SECTORS_PER_KB);
    
    *pFreeMB = ulFreeSizeKB/1024;
    *pFreePct = iPercentageFree;
}
/* [] END OF FILE */
