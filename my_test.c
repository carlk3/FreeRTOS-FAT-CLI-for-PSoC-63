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

#include "FreeRTOS.h"
#include "ff_stdio.h"
#include "ff_utils.h"

void vSampleFunction( char *pcFileName)
{
    // FF_FILE *ff_fopen( const char *pcFile, const char *pcMode );
    /* “w”	Open a file for reading and writing. 
    If the file already exists it will be truncated to zero length. 
    If the file does not already exist it will be created.*/
    FF_FILE *file = ff_fopen( pcFileName, "w" );
    configASSERT(file);
    char buf[8];
    memset(buf, '1', sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    // size_t ff_fwrite( const void *pvBuffer, size_t xSize, size_t xItems, FF_FILE * pxStream );
    size_t ni = ff_fwrite(buf, sizeof buf, 1, file);
    configASSERT(1 == ni);
    // long ff_ftell( FF_FILE *pxStream );
    /* Returns the current read/write position of an open file in the embedded FAT file system. 
    The position is returned as the number of bytes from the start of the file.*/
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    // int ff_feof( FF_FILE *pxStream );		
    /*Queries an open file in the embedded FAT file system to see if the file’s read/write pointer is at the end of the file.*/
    printf("%d: ff_feof(): %d\n", __LINE__, ff_feof(file));
    // void ff_rewind( FF_FILE *pxStream );
    ff_rewind(file);
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    printf("%d: ff_feof(): %d\n", __LINE__, ff_feof(file));
    // int ff_fclose( FF_FILE *pxStream );;
    int ec = ff_fclose(file);
    configASSERT(!ec);
    
    /* “a+”	Open a file for reading and writing. 
    If the file already exists then new data will be appended to the end of the file. 
    If the file does not already exist it will be created. */
    file = ff_fopen( pcFileName, "a+" );
    configASSERT(file);
    // int ff_feof( FF_FILE *pxStream );		
    /*Queries an open file in the embedded FAT file system to see if the file’s read/write pointer is at the end of the file.*/
    printf("%d: ff_feof(): %d\n", __LINE__, ff_feof(file));
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    
    memset(buf, '2', sizeof(buf));
    ni = ff_fwrite(buf, sizeof buf, 1, file);
    configASSERT(1 == ni);
    printf("%d: ff_feof(): %d\n", __LINE__, ff_feof(file));
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    ec = ff_fseek( file, 0, FF_SEEK_END );
    configASSERT(!ec);
    printf("%d: ff_feof(): %d\n", __LINE__, ff_feof(file));
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    ff_rewind(file);    
    printf("%d: ff_feof(): %d\n", __LINE__, ff_feof(file));
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    while (!ff_feof(file)) {
        ni = ff_fread(buf, sizeof buf, 1 , file);
        if (1 == ni) {
            printf("%.*s", sizeof(buf), buf);
        } else {
        	int error = stdioGET_ERRNO();
        	printf("%d: read: %s (%d)\n", __LINE__, strerror(error), error);
        } 
    };
    putchar('\n');
    ec = ff_fclose(file);
    configASSERT(!ec);
    
/*    
    
    
    // size_t ff_fread( void *pvBuffer, size_t xSize, size_t xItems, FF_FILE * pxStream );
    ni =  ff_fread(buf, sizeof buf, 1 , file);
    if (1 != ni) {
    	int error = stdioGET_ERRNO();
    	printf("%d: %s (%d)\n", __LINE__, strerror(error), error);
    }    
    ff_rewind(file);
    printf("%d: ff_feof(): %d\n", __LINE__, ff_feof(file));
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    //int ff_fseek( FF_FILE *pxStream, int iOffset, int iWhence );
    ec = ff_fseek( file, 0, FF_SEEK_END );
    configASSERT(!ec);
    printf("%d: ff_feof(): %d\n", __LINE__, ff_feof(file));
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    ec = ff_fclose(file);
    configASSERT(!ec);

    file = ff_fopen( pcFileName, "a+" );
    configASSERT(file);
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    char buf[] = {'a','b','c','d','e','f','g','h','i','j'};
    // size_t ff_fwrite( const void *pvBuffer, size_t xSize, size_t xItems, FF_FILE * pxStream );
    ni = ff_fwrite(buf, sizeof buf, 1, file);
    configASSERT(1 == ni);
    printf("%d: ff_feof(): %d\n", __LINE__, ff_feof(file));
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    ec = ff_fseek( file, 0, FF_SEEK_END );
    configASSERT(!ec);
    printf("%d: ff_feof(): %d\n", __LINE__, ff_feof(file));
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    ec = ff_fclose(file);
    configASSERT(!ec);
    
    
    
    
    char buf[] = {'a','b','c','d','e','f','g','h','i','j'};
    // size_t ff_fwrite( const void *pvBuffer, size_t xSize, size_t xItems, FF_FILE * pxStream );
    ni = ff_fwrite(buf, sizeof buf, 1, file);
    configASSERT(1 == ni);
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    ff_rewind(file);
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    ec = ff_fclose(file);
    configASSERT(!ec);
    
    ni = ff_fwrite(buf, sizeof buf, 1, file);
    configASSERT(1 == ni);
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    ni = ff_fwrite(buf, sizeof buf, 1, file);
    configASSERT(1 == ni);
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    ec = ff_fclose(file);
    configASSERT(!ec);
    file = ff_fopen( pcFileName, "a+" );
    configASSERT(file);
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    ec = ff_fseek( file, 0, FF_SEEK_SET );

//    while (!ff_feof(file)) {
//        ni = ff_fread(buf, sizeof buf, 1 , file);
//        if (1 == ni) {
//            printf("%.*s", sizeof(buf), buf);
//        } else {
//        	int error = stdioGET_ERRNO();
//        	printf("%d: read: %s (%d)\n", __LINE__, strerror(error), error);
//        } 
//    };
    putchar('\n');
    printf("%d: ff_feof(): %d\n", __LINE__, ff_feof(file));
    printf("%d: ff_ftell(): %ld\n", __LINE__, ff_ftell(file));
    
    */    
}

void my_test() {
    FF_Disk_t *pxDisk;
    mount(&pxDisk, "SDCard", "/SDCard");
    vSampleFunction("/SDCard/test.dat");
}

/* [] END OF FILE */
