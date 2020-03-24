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
#include <time.h>

extern void PrintDateTime(void);
extern void SetDateTime(
    const char *const dateStr,
    const char *const monthStr,
    const char *const yearStr,
    const char *const secStr,
    const char *const minStr,
    const char *const hourStr
);
extern time_t FreeRTOS_time( time_t *pxTime );

/* [] END OF FILE */
