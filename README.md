# FreeRTOS-FAT-CLI-for-PSoC-63
FreeRTOS+FAT Media Driver (ff_sddisk.c) for PSoC 6 using SPI, based on SDBlockDevice from Mbed OS 5. This example PSoC Creator project implements a FAT filesystem on SD card in an AdaLogger Shield on a PSoC® 6 BLE Pioneer Kit (CY8CKIT-062-BLE).

Prerequisites:
* PSoC® 6 BLE Pioneer Kit (CY8CKIT-062-BLE)
* PSoC Creator 4.3
* SD Card hardware
* FreeRTOS-Plus-FAT-191108a-MIT source files (https://www.freertos.org/FreeRTOS-Labs/downloads/FreeRTOS-Plus-FAT-160919a-MIT.zip)

See:
* FreeRTOS+FAT DOS Compatible Embedded FAT File System (https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_FAT/index.html)
* FreeRTOS+CLI (https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_Command_Line_Interface.html)
* PSoC® 6 BLE Pioneer Kit (https://www.cypress.com/documentation/development-kitsboards/psoc-6-ble-pioneer-kit-cy8ckit-062-ble) 
* AdaLogger Shield (https://www.adafruit.com/product/1141) (https://learn.adafruit.com/adafruit-data-logger-shield/overview)
* Mbed SDBlockDevice:
  * https://os.mbed.com/docs/mbed-os/v5.15/apis/sdblockdevice.html
  * mbed-os/components/storage/blockdevice/COMPONENT_SD/SDBlockDevice.cpp
