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

#include <stdio.h>
#include <stdarg.h> // varargs

#include "project.h"  // Generated code

// FreeRTOS:
#include "FreeRTOS.h"
#include <task.h>

extern bool die;
extern void CLI_Start();
extern void register_fs_tests();
extern void PrintDateTime(void);

void my_assert_func (const char *file, int line, const char *func, const char *pred) {
    fflush(stdout);
    printf("assertion \"%s\" failed: file \"%s\", line %d, function: %s\n", pred, file, line, func);
    fflush(stdout);        
#if !defined(NDEBUG)
    Cy_SysLib_AssertFailed(file, line); 
#else
    Cy_SysLib_Halt(0UL);
#endif    
}

// void init_hardware(void);

int main(void) {

	__enable_irq(); /* Enable global interrupts. */

	/* Place your initialization/startup code here (e.g. MyInst_Start()) */
    
//init_hardware();    
   
    CLI_Start();     
    register_fs_tests();        
    
    /* Initialize RTC */
    if(Cy_RTC_Init(&RTC_1_config) != CY_RTC_SUCCESS)
    {
        printf("RTC initialization failed \r\n");
        CY_ASSERT(0); /* If RTC initialization failed */
    }        
    PrintDateTime();   
    
    vTaskStartScheduler();  // Will never return
	configASSERT(!"It will never get here");
	Cy_SysLib_Halt(2);
}

/*******************************************************************************
* Function Name: void vApplicationIdleHook(void)
********************************************************************************
*
* Summary:
*  This function is called when the RTOS in idle mode
*    
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void vApplicationIdleHook(void)
{
    /* Enter sleep-mode */
    Cy_SysPm_Sleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
}


void my_printf(const char *pcFormat, ...) {
	char pcBuffer[256] = { 0 };
	va_list xArgs;
	va_start(xArgs, pcFormat);
	vsnprintf(pcBuffer, sizeof(pcBuffer), pcFormat, xArgs);
	va_end(xArgs); 
    fflush(stdout);
    printf("%p, %s: %s", xTaskGetCurrentTaskHandle(),
			pcTaskGetName(xTaskGetCurrentTaskHandle()), pcBuffer);
    fflush(stdout);    
}

/* [] END OF FILE */
