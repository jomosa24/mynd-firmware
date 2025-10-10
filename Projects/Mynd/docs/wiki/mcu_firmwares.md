# MCU Firmwares Explained

- **Bootloader (Requires flashing):** `mynd-bootloader.hex`
  - Simple stand-alone bootloader in hex binary format that consumes the update binary and handles the drag & drop update functionality
  - Can be flashed to the unit using a JLink device (not included)
    - Will sit indefinitely in update mode until a MCU firmware is provided via drag & drop update
- **Application (Requires flashing):** `mynd.hex`
  - Stand-alone application in hex binary format that **does not include the bootloader**; Mainly used for iterative development
  - Can be flashed to the unit using a JLink device (not included)
    - Drag & drop update functionality will not be available because there is no bootloader to jump to
    - **NOTE, do this if you are in need of flash space, have a JLink device, and can do without the drag & drop update functionality**
    - To return MYND to normal operation, flash the bootloader and perform drag & drop update with production firmware or just flash the **Complete** production firmware
- **Complete (Requires flashing):** `mynd-complete.hex`
  - Unified hex binary that merges the bootloader and the offset production application hex binaries
  - Can be flashed to the unit using a JLink device (not included)
- **Factory Complete (Requires flashing):** `mynd-factory-complete.hex`
  - Unified hex binary that merges the bootloader and the offset factory application hex binaries
  - Can be flashed to the unit using a JLink device (not included)
- **Factory (Requires drag & drop):** `mynd-factory-update-firmware-mcu.bin`
  - Firmware used to assign color and perform factory tests
  - Stand-alone factory application in binary format suitable for drag & drop update
  - Includes a tshell CLI (command-line interface) allowing for serial communication with the speaker's MCU
  - EQ and other features have been removed due to limited flash space!
- **Production (Requires drag & drop):** `mynd-update-firmware-mcu.bin`
  - Firmware that is delivered on all production units
  - Stand-alone production application in binary format suitable for drag & drop update
  - Includes a limited tshell CLI but requires direct access to MYND's UART pins (see **Method 1** below)
- **Offset**
  - Targets with offset suffix have their starting memory addresses shifted to make room for the bootloader in flash
    - `support/scripts/prepare_update.py` creates the update binaries from the offset binaries by merging them with the bootloader (see [mynd-firmware/Projects/Mynd/CMakeLists.txt](./Projects/Mynd/CMakeLists.txt))
- **ELF** `mynd` `mynd-factory`
  - Stand-alone applications, in ELF format, that **do not include the bootloader**
  - Can be used for debugging with the `JLinkGDBServer` JLink CLI tool along side the Cortex-Debug VSCode extension

**NOTE:**
 - In the **Build MYND Firmware** workflow, building the **Complete** targets specified under the `build_mynd` job requires `mynd-bootloader.hex` that is built by the `build_bootloader` job
   - This dependency is set by specifing `needs: build_bootloader` within the `build_mynd` job and ensures that the `build_bootloader` job always runs first so that the artifact is available