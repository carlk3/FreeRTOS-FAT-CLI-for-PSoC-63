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
    __disable_irq(); /* Disable global interrupts. */

    // Cy_SysLib_AssertFailed(file, line); 
    while(1);
}

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static – otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task’s
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task’s stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*———————————————————–*/

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static – otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task’s state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task’s stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/* [] END OF FILE */
