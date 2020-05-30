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
#include "FreeRTOS_CLI.h"

#include "uart_cli.h"

extern void register_fs_tests();

/*-----------------------------------------------------------*/
extern void my_test();
//extern void show_default();
static BaseType_t test(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
	(void) pcCommandString;
	(void) pcWriteBuffer;
	(void) xWriteBufferLen;

    my_test();

	return pdFALSE;
}
static const CLI_Command_Definition_t xTest = { 
    "test", /* The command string to type. */
    "\ntest:\n Quick test\n", 
    test, /* The function to run. */
    0 /* No parameters are expected. */
};
/*-----------------------------------------------------------*/


int main(void) {

	__enable_irq(); /* Enable global interrupts. */

	/* Place your initialization/startup code here (e.g. MyInst_Start()) */
    
// Don't do this, it sets time back to Date 01/01/00 Time 00:00:00    
//    /* Initialize RTC */
//    RTC_1_Start();
    
    // Configures the supercapacitor charger circuit:
    Cy_SysPm_BackupSuperCapCharge(CY_SYSPM_SC_CHARGE_ENABLE);    
  
    CLI_Start();     
    register_fs_tests();   
	FreeRTOS_CLIRegisterCommand(&xTest);            
    
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

void vApplicationMallocFailedHook( void ) {
    fflush(stdout);
    puts("Malloc failed!");
    fflush(stdout);        
    Cy_SysLib_Halt(0UL);    
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, signed char *pcTaskName ) {
    /* The stack space has been exceeded for a task, considering allocating more. */
    fflush(stdout);
    puts("Out of stack space!");
    printf("Task: %p %s\n", xTask, pcTaskName);
    fflush(stdout);            
    Cy_SysLib_Halt(0UL);
}

void my_printf(const char *pcFormat, ...) {
	char pcBuffer[256] = { 0 };
	va_list xArgs;
	va_start(xArgs, pcFormat);
	vsnprintf(pcBuffer, sizeof(pcBuffer), pcFormat, xArgs);
	va_end(xArgs); 
    fflush(stdout);
//    printf("%p, %s: %s", xTaskGetCurrentTaskHandle(),
//			pcTaskGetName(xTaskGetCurrentTaskHandle()), pcBuffer);
    printf("%s", pcBuffer);
    fflush(stdout);    
}


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
/* [] END OF FILE */
