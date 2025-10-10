# How To Perform a Drag & Drop Update
#### Follow these steps to update the MCU Firmware:

1. **Power on the unit**
    - Wait for the power on sound icon 
2. **Enter update mode:**  
    - Hold down both the **Power** and **Volume Minus** buttons for 8–10 seconds until the Power LED begins periodically flashing red.  
    - The unit is now in update mode and will remain in this mode until the update is completed or a hardware reset is performed by holding **Power** + **Plus** + **Minus** for 8–10 seconds.
3. **Connect to computer:**  
    - Plug the unit into a computer using a USB-C cable with data transfer capability (note: some cables may only provide power).
4. **Access the TEUFEL volume:**  
    - The computer will automatically recognize the speaker and display it in the file explorer or on the desktop as a volume named **TEUFEL** (similar to a USB drive).
5. **Obtain the firmware binary:**  
    - Unzip the downloaded build artifacts and select the preferred target binary from the build artifacts (see MCU Firmwares below).
6. **Drag & drop the update:**  
    - Open both the **TEUFEL** volume folder and the build artifacts folder side by side.  
    - Select the firmware file, drag it over the **TEUFEL** folder window, and drop/release it.
7. **Firmware update process:**  
    - The TEUFEL volume will automatically consume the file, apply the MCU firmware update, and reboot when finished.