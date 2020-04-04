# FreeRTOS-FAT-CLI-for-PSoC-63
FreeRTOS+FAT Media Driver (ff_sddisk.c) for PSoC 6 using SPI, based on SDBlockDevice from Mbed OS 5. This example PSoC Creator project implements a FAT filesystem on SD card in an AdaLogger Shield on a PSoC® 6 BLE Pioneer Kit (CY8CKIT-062-BLE).

Features:
* Supports multiple SD Cards
* Supports multiple SPIs
* Supports multiple SD Cards per SPI
* Detects and responds to SD Card removal
* Supports Real Time Clock for maintaining file and directory time stamps
* Integrated with the FreeRTOS component in the Cypress Peripheral Driver Library (PDL)

Limitations:
* No support for multiple partitions on an SD Card

Prerequisites:
* PSoC® 6 BLE Pioneer Kit (CY8CKIT-062-BLE)
* PSoC Creator 4.3 
* SD Card hardware
* FreeRTOS-Plus-FAT-191108a-MIT source files 
  * (https://www.freertos.org/FreeRTOS-Labs/downloads/FreeRTOS-Plus-FAT-160919a-MIT.zip)
  * or get the latest at FreeRTOS/Lab-Project-FreeRTOS-FAT (https://github.com/FreeRTOS/Lab-Project-FreeRTOS-FAT)

Getting Started; Hardware
* Decide how to physically attach your SD Card(s). I used the AdaFruit AdaLogger Shield. Later I added a second card in the breadboard area using a SparkFun microSD Transflash Breakout (https://www.sparkfun.com/products/544).
* Figure out pin assignments. The AdaLogger hardware can be configured in various ways. Check for pin conflicts if you want to stack the AdaLogger and the E-ink display shield board. You will probably need to run a jumper wire for the Card Detect switch.
* In PSoC Creator, customize to match hardware:
  * Schematic in TopDesign.cysch 
  * Design Wide Resources/Pins in FreeRTOS+FAT+CLI.cydwr 
 
Getting Started; Software
* Install source code (outline):
  * md Lab-Project-FreeRTOS-FAT
  * cd Lab-Project-FreeRTOS-FAT
  * git init
  * git remote add origin https://github.com/FreeRTOS/Lab-Project-FreeRTOS-FAT
  * git pull origin master
  * cd ..
  * md FreeRTOS+FAT+CLI.cydsn
  * cd FreeRTOS+FAT+CLI.cydsn
  * git init
  * git remote add origin https://github.com/carlk3/FreeRTOS-FAT-CLI-for-PSoC-63.git
  * git pull origin master
* Open FreeRTOS+FAT+CLI.cyprj with PSoC Creator
* Tailor hw_config.c to match hardware
* Program the device

Getting Started; Running
* Connect a terminal such as PuTTY at 115200 baud, 8 data, 1 stop, no parity. You can find the COM port in Windows Device Manager.
* Type "help" to see available commands in the CLI.
* Try a command like "simple SDCard" to run a test.

See:
* FreeRTOS+FAT DOS Compatible Embedded FAT File System (https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_FAT/index.html)
* FreeRTOS+CLI (https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_Command_Line_Interface.html)
* PSoC® 6 BLE Pioneer Kit (https://www.cypress.com/documentation/development-kitsboards/psoc-6-ble-pioneer-kit-cy8ckit-062-ble) 
* PSoC Creator 4.3 https://www.cypress.com/products/psoc-creator-integrated-design-environment-ide
* AdaLogger Shield (https://www.adafruit.com/product/1141) (https://learn.adafruit.com/adafruit-data-logger-shield/overview)
* Mbed SDBlockDevice:
  * https://os.mbed.com/docs/mbed-os/v5.15/apis/sdblockdevice.html
  * mbed-os/components/storage/blockdevice/COMPONENT_SD/SDBlockDevice.cpp

See the Project tab in GitHub for development backlog items.
