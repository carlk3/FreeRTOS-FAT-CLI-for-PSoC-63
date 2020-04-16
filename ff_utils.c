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

#include "ff_utils.h"

#define HIDDEN_SECTOR_COUNT     8
#define PRIMARY_PARTITIONS      1
#define PARTITION_NUMBER        0

static FF_Error_t prvPartitionAndFormatDisk( FF_Disk_t *pxDisk )
{
FF_PartitionParameters_t xPartition;
FF_Error_t xError;

	/* Media cannot be used until it has been partitioned.  In this
	case a single partition is to be created that fills all available space – so
	by clearing the xPartition structure to zero. */
	memset( &xPartition, 0x00, sizeof( xPartition ) );
	xPartition.ulSectorCount = pxDisk->ulNumberOfSectors;
	xPartition.ulHiddenSectors = HIDDEN_SECTOR_COUNT;
	xPartition.xPrimaryCount = PRIMARY_PARTITIONS;
	xPartition.eSizeType = eSizeIsQuota;

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
	*ppxDisk = FF_SDDiskInit( devName );
	if (!*ppxDisk) {
		return false;
	}
	// BaseType_t FF_SDDiskMount( FF_Disk_t *pDisk );
	FF_Error_t xError = FF_SDDiskMount(*ppxDisk);
	if (FF_isERR( xError ) != pdFALSE) {
		FF_PRINTF("FF_SDDiskMount: %s\n", (const char *) FF_GetErrMessage(xError));
		return false;
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
	FF_SDDiskDelete(pxDisk);
	pxDisk = 0;
    
}
/* [] END OF FILE */