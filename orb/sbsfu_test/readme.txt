/**
  @page Secure Firmware Update - User Application Demo

  @verbatim
  ******************** (C) COPYRIGHT 2017 STMicroelectronics *******************
  * @file    readme.txt
  * @brief   This application shows a User Application
  ******************************************************************************
  *
  * Copyright (c) 2017 STMicroelectronics. All rights reserved.
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                               www.st.com/SLA0044
  *
  ******************************************************************************
  @endverbatim

@par Application Description

This application demonstrates firmware download capabilities and provides a set of functions to test the active
protections offered by Secure Boot and Secure Engine.
A terminal connected with the board via VCOM is needed to communicate with the board and to select which feature
to demonstrate.

For more details, refer to UM2262 "Getting started with SBSFU - software expansion for STM32Cube" available from
the STMicroelectronics microcontroller website www.st.com.

@par Directory contents

   - 2_Images_UserApp/Src/com.c                      Communication module file
   - 2_Images_UserApp/Src/common.c                   Common module file
   - 2_Images_UserApp/Src/flash_if.c                 Flash interface file
   - 2_Images_UserApp/Src/fw_update_app.c            Firmware update application
   - 2_Images_UserApp/Src/main.c                     Main program
   - 2_Images_UserApp/Src/se_user_code.c             Call user defined services running in Secure Engine
   - 2_Images_UserApp/Src/sfu_app_new_image.c        Manage the new firmware image storage and installation
   - 2_Images_UserApp/Src/stm32g4xx_it.c             Interrupt handlers
   - 2_Images_UserApp/Src/system_stm32g4xx.c         STM32 system file
   - 2_Images_UserApp/Src/test_protections.c         Protection test
   - 2_Images_UserApp/Src/ymodem.c                   Ymodem communication module
   - 2_Images_UserApp/Inc/com.h                      Header for com.c file
   - 2_Images_UserApp/Inc/common.h                   Header for common.c file
   - 2_Images_UserApp/Inc/flash_if.h                 Header for flash_if.c file
   - 2_Images_UserApp/Inc/fw_update_app.h            Header for fw_update_app.c file
   - 2_Images_UserApp/Inc/main.h                     Header for main.c file
   - 2_Images_UserApp/Inc/se_user_code.h             Header file for se_user_code.c
   - 2_Images_UserApp/Inc/stm32g4xx_nucleo_conf.h    BSP configuration file
   - 2_Images_UserApp/Inc/sfu_app_new_image.h        Header file for sfu_app_new_image.c
   - 2_Images_UserApp/Inc/stm32g4xx_conf.h           HAL configuration file
   - 2_Images_UserApp/Inc/stm32g4xx_it.h             Header for stm32g4xx_it.c file
   - 2_Images_UserApp/Inc/test_protections.h         Header for test_protections.c file
   - 2_Images_UserApp/Inc/ymodem.h                   Header for ymodem.c file

@par Hardware and Software environment

   - This example runs on STM32G474xx devices.
   - This example has been tested with NUCLEO-G474RE RevC board and can be easily tailored to any other supported device and
     development board.
   - An up-to-date version of ST-LINK firmware is required. Upgrading ST-LINK firmware is a feature provided by
     STM32Cube programmer available on www.st.com.
   - This example is based on se_interface_application.o module exported by SBSFU project.
   - This example needs a terminal emulator.
   - Microsoft Windows has a limitation whereby paths to files and directories cannot be longer than 256 characters.
     Paths to files exceeding that limits cause tools (e.g. compilers, shell scripts) to fail reading from or writing to
     such files.
     As a workaround, it is advised to use the subst.exe command from within a command prompt to set up a local drive
     out of an existing directory on the hard drive, such as:
     C:\> subst X: <PATH_TO_CUBEFW>\Firmware

@par IDE postbuild script

In order to ease the development process, a postbuild script ("postbuild.bat") is integrated in each IDE project.
This postbuild script:
   - is generated when compiling the Secure Engine Core project,
   - prepares the firmware image of the user application to be installed in the device.
   - prepares the new portion of firmware image: part of firmware image which changed vs a reference firmware image.
     The reference firmware image is provided to the postbuild script by manually copying the UserApp.bin of the running
     application into RefUserApp.bin (in folder 2_Images_UserApp/IDE).

     As an example, once original user application has been build, downloaded and installed, you may create the
     RefUserApp.bin, then change UserAppId constant from 'A' to 'B' in source file main.c, then build again the user
     application.
     At post processing stage, the portion of code containing the UserAppId will be identified as a difference vs the
     reference binary file and a PartialUserApp.sfb will be generated in 2_Images_UserApp/Binary folder.

A known limitation of this integration occurs when you update the firmware version (parameter of postbuild.bat script).
The IDE does not track this update so you need to force the rebuild of the project manually.

@par How to use it ?

Refer to SBSFU readme and follow steps by steps instructions.

Once executed, this user application gives access to a menu which allows:
   1 - to download a new firmware
   2 - to test protections (Secure user memory, IWDG, TAMPER)
   3 - to demonstrate how to call user defined services running in Secure Engine
   4 - to provide access to multiple images feature
   5 - to validate a firmware image at first start-up

1. Pressing 1 allows to download a new firmware.
Send the user encrypted firmware file (.sfb) with Tera Term by using menu "File > Transfer > YMODEM > Send..."
After the download, the system reboots.
The downloaded encrypted firmware is detected, decrypted, installed and executed.

The downloaded image can be either a complete UserApp.sfb or a PartialUserApp.sfb.
To download a PartialUserApp.sfb, it is mandatory to have previously installed the UserApp.sfb identified as reference
into the device.

2. Pressing 2 allows to test protections.
   - CORRUPT IMAGE test (#1): causes a signature verification failure of the selected firmware image at next boot.
   - Secure user memory protection test (#2): cause an error trying to access protected code.
   - IWDG test (#3): causes a reset simulating a deadlock by not refreshing the watchdog.
   - TAMPER test (#4): causes a reset if a tamper event is detected. In order to generate a tamper event, user has to
     connect PA0 (CN11.1) to GND (It may be enough to put your finger close to PA0 (CN11.1)).

3. Pressing 3 allows to call user defined services running in Secure Engine.
With secure user memory protection, you should not be able to call a SE service and get stuck.
Press the RESET button to restart the device.

4. This menu is dedicated to multiple images feature.
Feature not available as there is only 1 download area configured.

5. This menu is dedicated to image validation.
Feature available under ENABLE_IMAGE_STATE_HANDLING compilation switch, not activated in this example.


Note1 : There is only 1 active slot configured in this example.
Note2 : for Linux users Minicom can be used but to do so you need to compile the UserApp project with the MINICOM_YMODEM
        switch enabled (ymodem.h)


 * <h3><center>&copy; COPYRIGHT STMicroelectronics</center></h3>
 */
