/******************************************************************************
* File Name: main_cm4.c
* Version 1.10
*
* Description:
*  RTC Code Example. This code example demonstrates how to read and write the
*  current time using a RTC component. The UART interface is used to input a 
*  command and and print the result on the terminal.
* 
* Related Documents: CE216825_PSoC6_RTC_Basics.pdf
*
* Hardware Dependency:
*  1. PSoC 6 MCU device
*  2. CY8CKIT-062-BLE Pioneer Kit
*
*******************************************************************************
* Copyright (2017-2018), Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* (“Software”), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries (“Cypress”) and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software (“EULA”).
*
* If no EULA applies, Cypress hereby grants you a personal, nonexclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress’s integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, 
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress 
* reserves the right to make changes to the Software without notice. Cypress 
* does not assume any liability arising out of the application or use of the 
* Software or any product or circuit described in the Software. Cypress does 
* not authorize its products for use in any products where a malfunction or 
* failure of the Cypress product may reasonably be expected to result in 
* significant property damage, injury or death (“High Risk Product”). By 
* including Cypress’s product in a High Risk Product, the manufacturer of such 
* system or application assumes all risk of such use and in doing so agrees to 
* indemnify Cypress against all liability.
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "project.h"
#include "stdio_user.h"
#include "rtc.h"

/* Macros used to store commands */
#define RTC_CMD_GET_DATE_TIME   ('1')
#define RTC_CMD_SET_DATE_TIME   ('2')

#define MAX_LENGTH              (4u)
#define VALID_DAY_LENGTH        (2u)
#define VALID_MONTH_LENGTH      (2u)
#define VALID_SHORT_YEAR_LENGTH (2u)
#define VALID_LONG_YEAR_LENGTH  (4u)

/*****************************************************/
/*              Function prototype                   */
/*****************************************************/
static bool ValidateDateTime(uint32_t year, uint32_t month, uint32_t date, \
            uint32_t sec, uint32_t min, uint32_t hour);
static inline bool IsLeapYear(uint32_t );
void synch_time();

/*
Reading RTC User Registers
To start a read transaction, the firmware should set the
READ bit in the BACKUP_RTC_RW register. When this bit
is set, the RTC registers will be copied to user registers and
frozen so that a coherent RTC value can safely be read by
the firmware. The read transaction is completed by clearing
the READ bit.
The READ bit cannot be set if:
■ RTC is still busy with a previous operation (that is, the
RTC_BUSY bit in the BACKUP_STATUS register is set)
■ WRITE bit in the BACKUP_RTC_RW register is set
The firmware should verify that the above bits are not set
before setting the READ bit.
*/    
static void readRTCUserRegisters(void)
{    
    while ((CY_RTC_BUSY == Cy_RTC_GetSyncStatus()) || (_FLD2BOOL(BACKUP_RTC_RW_WRITE, BACKUP_RTC_RW)))
        ; // Spin
    /* Setting RTC Read bit */
    BACKUP_RTC_RW = BACKUP_RTC_RW_READ_Msk;

    /* Clearing RTC Read bit */
    BACKUP_RTC_RW = 0U;
}

void my_RTC_GetDateAndTime(cy_stc_rtc_config_t* dateTime)
{
    uint32_t tmpTime;
    uint32_t tmpDate;

    CY_ASSERT_L1(NULL != dateTime);

    /* Read the current RTC time and date to validate the input parameters */
    readRTCUserRegisters();

    /* Write the AHB RTC registers date and time into the local variables and 
    * updating the dateTime structure elements
    */
    tmpTime = BACKUP_RTC_TIME;
    tmpDate = BACKUP_RTC_DATE;

    dateTime->sec = Cy_RTC_ConvertBcdToDec(_FLD2VAL(BACKUP_RTC_TIME_RTC_SEC, tmpTime));
    dateTime->min = Cy_RTC_ConvertBcdToDec(_FLD2VAL(BACKUP_RTC_TIME_RTC_MIN, tmpTime));
    dateTime->hrFormat = ((_FLD2BOOL(BACKUP_RTC_TIME_CTRL_12HR, tmpTime)) ? CY_RTC_12_HOURS : CY_RTC_24_HOURS);

    /* Read the current hour mode to know how many hour bits should be converted
    * In the 24-hour mode, the hour value is presented in [21:16] bits in the 
    * BCD format.
    * In the 12-hour mode the hour value is presented in [20:16] bits in the BCD
    * format and bit [21] is present: 0 - AM; 1 - PM. 
    */
    if (dateTime->hrFormat != CY_RTC_24_HOURS)
    {
        dateTime->hour = 
        Cy_RTC_ConvertBcdToDec((tmpTime & CY_RTC_BACKUP_RTC_TIME_RTC_12HOUR) >> BACKUP_RTC_TIME_RTC_HOUR_Pos);

        dateTime->amPm = ((0U != (tmpTime & CY_RTC_BACKUP_RTC_TIME_RTC_PM)) ? CY_RTC_PM : CY_RTC_AM);
    }
    else
    {
        dateTime->hour = Cy_RTC_ConvertBcdToDec(_FLD2VAL(BACKUP_RTC_TIME_RTC_HOUR, tmpTime));

        dateTime->amPm = CY_RTC_AM;
    }
    dateTime->dayOfWeek = Cy_RTC_ConvertBcdToDec(_FLD2VAL(BACKUP_RTC_TIME_RTC_DAY, tmpTime));
    
    dateTime->date  = Cy_RTC_ConvertBcdToDec(_FLD2VAL(BACKUP_RTC_DATE_RTC_DATE, tmpDate));
    dateTime->month = Cy_RTC_ConvertBcdToDec(_FLD2VAL(BACKUP_RTC_DATE_RTC_MON, tmpDate));
    dateTime->year  = Cy_RTC_ConvertBcdToDec(_FLD2VAL(BACKUP_RTC_DATE_RTC_YEAR, tmpDate));
}



/*******************************************************************************
* Function Name: PrintDateTime
********************************************************************************
* Summary:
*  This function prints current date and time to UART
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void PrintDateTime(void)
{
    /* Variable used to store date and time information */
    cy_stc_rtc_config_t dateTime;
    
    /*Get the current RTC time and date */
    my_RTC_GetDateAndTime(&dateTime);
    
    printf("\r\nCurrent date and time\r\n");
    printf("Date %02u/%02u/%02u\r\n",(uint16_t) dateTime.date, 
            (uint16_t) dateTime.month, (uint16_t) dateTime.year);
    printf("Time %02u:%02u:%02u\r\n", (uint16_t) dateTime.hour, 
            (uint16_t) dateTime.min, (uint16_t) dateTime.sec);
}

/*******************************************************************************
* Function Name: SetDateTime
********************************************************************************
* Summary:
*  This function sets new date and time.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
//static 
void SetDateTime(
    const char *const dateStr,
    const char *const monthStr,
    const char *const yearStr,
    const char *const secStr,
    const char *const minStr,
    const char *const hourStr)
{
    
    /* Variables used to store date and time information */
    uint32_t date, month, year, sec, min, hour;
        
    /* Convert string input to decimal */
    date    = atoi(dateStr);
    month   = atoi(monthStr);
    year    = atoi(yearStr);
    sec     = atoi(secStr);
    min     = atoi(minStr);
    hour    = atoi(hourStr);
    
    if(year > CY_RTC_MAX_YEAR) /* If user input 4 digits Year information, set 2 digits Year */
    {
        year = year % 100u;
    }
    
    if(ValidateDateTime(sec, min, hour, date, month, year))
    {
        /* Set date and time */
        if( Cy_RTC_SetDateAndTimeDirect(sec, min, hour, date, 
            month, year ) != CY_RTC_SUCCESS)
        {
            printf("Failed to update date and time\r\n");
        }
        else
        {
            synch_time();
            printf("\r\nDate and Time updated !!\r\n");
            PrintDateTime();
        }
    }
    else
    {
        printf("\r\nInvalid values! Please enter the values in specified format\r\n");
    }
}

/*******************************************************************************
* Function Name: ValidateDateTime
********************************************************************************
* Summary:
*  This function validates date and time value.

*
* Parameters:
*  sec:  The second valid range is [0-59].
*  min:  The minute valid range is [0-59].
*  hour: The hour valid range is [0-23].
*  date: The date valid range is [1-31], if the month of February is 
*        selected as the Month parameter, then the valid range is [0-29].
*  month: The month valid range is [1-12].
*  year: The year valid range is [0-99].
*
* Return:
*  false - invalid ; true - valid
*
*******************************************************************************/
static bool ValidateDateTime(uint32_t sec, uint32_t min, uint32_t hour, \
                      uint32_t date, uint32_t month, uint32_t year)
{
    /* Variable used to store days in months table */
    static uint8_t daysInMonthTable[CY_RTC_MONTHS_PER_YEAR] = {CY_RTC_DAYS_IN_JANUARY,
                                                            CY_RTC_DAYS_IN_FEBRUARY,
                                                            CY_RTC_DAYS_IN_MARCH,
                                                            CY_RTC_DAYS_IN_APRIL,
                                                            CY_RTC_DAYS_IN_MAY,
                                                            CY_RTC_DAYS_IN_JUNE,
                                                            CY_RTC_DAYS_IN_JULY,
                                                            CY_RTC_DAYS_IN_AUGUST,
                                                            CY_RTC_DAYS_IN_SEPTEMBER,
                                                            CY_RTC_DAYS_IN_OCTOBER,
                                                            CY_RTC_DAYS_IN_NOVEMBER,
                                                            CY_RTC_DAYS_IN_DECEMBER};
    uint8_t daysInMonth;
    bool status = true;
        
    status &= CY_RTC_IS_SEC_VALID(sec);
    status &= CY_RTC_IS_MIN_VALID(min);
    status &= CY_RTC_IS_HOUR_VALID(hour);
    status &= CY_RTC_IS_MONTH_VALID(month);
    status &= CY_RTC_IS_YEAR_SHORT_VALID(year);
    
    if(status)
    {
        daysInMonth = daysInMonthTable[month - 1];
        
        if(IsLeapYear(year + CY_RTC_TWO_THOUSAND_YEARS) && (month == CY_RTC_FEBRUARY))
        {        
            daysInMonth++;
        }
        status &= (date > 0U) && (date <= daysInMonth);
    }
    return status;
}

/*******************************************************************************
* Function Name: IsLeapYear
********************************************************************************
* Summary:
*  This function checks whether the year passed through the parameter is leap or
*  not.  Leap year is identified as a year that is a multiple of 4 or 400 but not 
*  100.
*
* Parameters:
*  year: The year to be checked
*
* Return:
*  false - The year is not leap; true - The year is leap.
*
*******************************************************************************/
static inline bool IsLeapYear(uint32_t year)
{
    return(((0U == (year % 4UL)) && (0U != (year % 100UL))) || (0U == (year % 400UL)));
}

static time_t offset; // Seconds to add to TimerTick for epoch time

void synch_time() {
    struct tm timeinfo;
    cy_stc_rtc_config_t dateTime;
    
    /*Get the current RTC time and date */
    my_RTC_GetDateAndTime(&dateTime);
    
    // The values of the members tm_wday and tm_yday are ignored
    
//Member	Type	Meaning	                Range
//tm_sec	int	seconds after the minute	0-61*
    timeinfo.tm_sec = dateTime.sec;    
//tm_min	int	minutes after the hour	    0-59
    timeinfo.tm_min = dateTime.min;
//tm_hour	int	hours since midnight	    0-23
    if (CY_RTC_12_HOURS == dateTime.hrFormat)
        if (CY_RTC_AM == dateTime.amPm) 
            timeinfo.tm_hour = dateTime.hour - 1;           
        else       
           timeinfo.tm_hour = dateTime.hour - 1 + 12;
    else
        timeinfo.tm_hour = dateTime.hour;        
//  tm_mday	int	day of the month	        1-31
    timeinfo.tm_mday = dateTime.date;
//  tm_mon	int	months since January	    0-11
    timeinfo.tm_mon = dateTime.month - 1;
//  tm_year	int	years since 1900	
    timeinfo.tm_year = dateTime.year + 100;
//  tm_wday	int	days since Sunday	    0-6
    timeinfo.tm_wday = -1;
//  tm_yday	int	days since January 1	0-365
    timeinfo.tm_wday = -1;
//  tm_isdst	int	Daylight Saving Time flag	
//  The Daylight Saving Time flag (tm_isdst) is greater than zero 
//    if Daylight Saving Time is in effect, 
//    zero if Daylight Saving Time is not in effect, 
//    and less than zero if the information is not available.    
    timeinfo.tm_isdst = -1;
        
    time_t epochtime = mktime ( &timeinfo );        // s since epoch    
    TickType_t xTicksNow = xTaskGetTickCount(); // ms since start
    offset = epochtime - (xTicksNow / configTICK_RATE_HZ);
}
time_t FreeRTOS_time( time_t *pxTime ) {
    if (!offset) 
        synch_time();
    
    time_t epochtime = (xTaskGetTickCount() / configTICK_RATE_HZ) + offset;     
    
    if (0 == epochtime % 60*60) // Synch to RTC once an hour
        synch_time();
        
    if (pxTime)
        *pxTime = epochtime;
    
    return epochtime;    
}

time_t new_FreeRTOS_time( time_t *pxTime ) { // FIXME
    struct tm timeinfo;
    cy_stc_rtc_config_t dateTime;
            
    /*Get the current RTC time and date */
    my_RTC_GetDateAndTime(&dateTime);
    
    // The values of the members tm_wday and tm_yday are ignored
    
//Member	Type	Meaning	                Range
//tm_sec	int	seconds after the minute	0-61*
    timeinfo.tm_sec = dateTime.sec;    
//tm_min	int	minutes after the hour	    0-59
    timeinfo.tm_min = dateTime.min;
//tm_hour	int	hours since midnight	    0-23
    if (CY_RTC_12_HOURS == dateTime.hrFormat)
        if (CY_RTC_AM == dateTime.amPm) 
            timeinfo.tm_hour = dateTime.hour - 1;           
        else       
           timeinfo.tm_hour = dateTime.hour - 1 + 12;
    else
        timeinfo.tm_hour = dateTime.hour;        
//  tm_mday	int	day of the month	        1-31
    timeinfo.tm_mday = dateTime.date;
//  tm_mon	int	months since January	    0-11
    timeinfo.tm_mon = dateTime.month - 1;
//  tm_year	int	years since 1900	
    timeinfo.tm_year = dateTime.year + 100;
//  tm_wday	int	days since Sunday	    0-6
    timeinfo.tm_wday = -1;
//  tm_yday	int	days since January 1	0-365
    timeinfo.tm_wday = -1;
//  tm_isdst	int	Daylight Saving Time flag	
//  The Daylight Saving Time flag (tm_isdst) is greater than zero 
//    if Daylight Saving Time is in effect, 
//    zero if Daylight Saving Time is not in effect, 
//    and less than zero if the information is not available.    
    timeinfo.tm_isdst = -1;
        
    time_t epochtime = mktime ( &timeinfo );        // s since epoch    
    
    if (pxTime)
        *pxTime = epochtime;
    
    return epochtime;    
}

/* [] END OF FILE */
