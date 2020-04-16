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

#ifndef _FS_UTILS_H
#define _FS_UTILS_H   

#include <stdbool.h>
#include "ff_ioman.h"

bool format(FF_Disk_t **ppxDisk, const char *const devName);
bool mount(FF_Disk_t **ppxDisk, const char *const devName, const char *const path);
void unmount(FF_Disk_t *pxDisk, const char *pcPath);
void eject(const char *const name);

#endif
/* [] END OF FILE */
