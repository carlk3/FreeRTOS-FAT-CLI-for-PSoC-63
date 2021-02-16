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
#include <stdbool.h>

void PrintDateTime();
bool ValidateDateTime(uint32_t sec, uint32_t min, uint32_t hour, uint32_t date, uint32_t month, uint32_t year);
void SetDateTime(
    const char *const dateStr,
    const char *const monthStr,
    const char *const yearStr,
    const char *const secStr,
    const char *const minStr,
    const char *const hourStr
);
time_t FreeRTOS_time( time_t *pxTime );
void synch_time();

// For timestamp without function call
extern time_t prev_epochtime; 

/* [] END OF FILE */
