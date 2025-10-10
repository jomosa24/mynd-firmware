# Clone
```shell
git clone --recurse-submodules git@github.com:teufelaudio/mynd-firmware.git
```
# Build
```shell
mkdir build && cd build
cmake -DPROJECT=Mynd -DTOOLCHAIN_PREFIX=<path to compiler> -DCMAKE_BUILD_TYPE=Release ..
make mynd-update-firmware-mcu
```
# Project structure
 - `drivers`            - Basic stm I/O drivers
 - `firmwares`          - Third-party firmwares (Bluetooth, USB-PD controller)
 - `gitversion`         - Version string generator
 - `Projects`
   - `Mynd`             - Source code for MCU firmware
   - `MyndBootloader`   - Source code for bootloader
 - `sdk`                - Board support package
 - `support`
   - `cmake`            - Cmake helpers for stm32 components and other libraries
   - `docker-build`     - Configuration for docker build
   - `keys`             - Keys to sign firmware update file (Only for tests! They are not used)
   - `scripts`          - Helper scripts for build and CI/CD
   - `svd`              - Hardware description file - internal registers outline


# Building the MYND Firmware with GitHub Actions

Want to easily build the MYND firmware and test your changes on your personal speaker? Follow these steps:

### Step 1: Fork and Access Actions

- Fork the `mynd-firmware` repository
- Navigate to the **Actions** tab at the top of your `mynd-firmware` GitHub webpage

### Step 2: Setup Build Environment

- In the **Actions** tab, select the **Setup MYND Build Environment** workflow from the left pane
- Click **Run workflow**
  - This workflow creates a pipeline that builds a Docker container, with a preconfigured environment for building MYND firmware, and publishes it to your repo's GHCR (GitHub Container Registry)
    - **Important:** You must run this workflow before any other builds; otherwise, build pipelines will fail on future pushes or pull requests.  
      - Run this workflow only once, unless you make changes to the Dockerfile at `support/docker-build/docker-config/Dockerfile`, in which case you need to run it again.

### Step 3: Build Firmware

- In the **Actions** tab, select **Build MYND Firmware** workflow from the left pane
- Click **Run workflow**
  - This builds the MYND firmware binaries from the `main` branch
  - The workflow also runs automatically on pushes or pull requests to your repo's `main` branch
  - You can monitor progress by clicking on the pipeline name

### Step 4: Download Artifacts

- After the workflow completes, click on the latest pipeline
- Wait a few minutes for build artifacts to appear under the **Artifacts** section at the bottom of the page and select the download option

### Step 5: Perform Drag & Drop MCU Update
- **Power on the unit**
- **Enter update mode:**  
   Hold down both the **Power** and **Volume Minus** buttons for 8–10 seconds until the Power LED begins periodically flashing red.  
     - The unit is now in update mode and will remain in this mode until the update is completed or a hardware reset is performed by holding **Power**+**Plus**+**Minus** for 8-10 seconds.
- **Connect to computer:**  
  Plug the unit into a computer using a USB-C cable with data transfer capability (some cheaper cables may only provide power).
- **Access the TEUFEL volume:**  
  The computer will automatically recognize the speaker and display it in the file explorer and/or the desktop as a volume named **TEUFEL** (similar to how USB drives typically appear).
- **Obtain the firmware binary:**  
  Unzip the downloaded build artifacts and select the preferred target binary from the build artifacts (see MCU Firmwares below)
- **Drag & drop the update:**  
  Open both the **TEUFEL** volume folder and the build artifacts folder side by side, select the firmware, drag it over the **TEUFEL** folder window, and drop/release it.
- **Firmware update process:**  
  The TEUFEL volume will automatically consume the file, apply the MCU firmware update, and reboot when finished.

### Repeat steps 4 and 5 after pushing changes to the `main` branch ###


**NOTE:**

- After forking, the **Build MYND Firmware** workflow will create a pipeline on every push or pull request to `main`, but will fail until you complete **Step 2** (Setup Build Environment), because it will not find the required docker container in the GHCR
- Once the build environment is set up, you can:
  - Edit code directly in the GitHub web IDE by replacing `.com` with `.dev` in the URL:  
    `https://github.com/<your_gh_username>/mynd-firmware` → `https://github.dev/<your_gh_username>/mynd-firmware`
  - Or, clone the repo locally to use a preferred IDE and push changes using Git
- Workflows, and the pipelines they create, are defined with YAML scripts at [mynd-firmware/.github/workflows](https://github.com/teufelaudio/mynd-firmware/tree/main/.github/workflows)

# MCU Firmwares

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
    - `support/scripts/prepare_update.py` creates the update binaries from the offset binaries by merging them with the bootloader (see [mynd-firmware/Projects/Mynd/CMakeLists.txt](https://github.com/teufelaudio/mynd-firmware/blob/main/Projects/Mynd/CMakeLists.txt))
- **ELF** `mynd` `mynd-factory`
  - Stand-alone applications, in ELF format, that **do not include the bootloader**
  - Can be used for debugging with the `JLinkGDBServer` JLink CLI tool along side the Cortex-Debug VSCode extension

**NOTE:**
 - In the **Build MYND Firmware** workflow, building the **Complete** targets specified under the `build_mynd` job requires `mynd-bootloader.hex` that is built by the `build_bootloader` job
   - This dependency is set by specifing `needs: build_bootloader` within the `build_mynd` job and ensures that the `build_bootloader` job always runs first so that the artifact is available
  
### For the tech-savvy who wish to experiment with the firmware CLI, there are two methods to get started: 

#### Method 1

- This method can be performed with either firmware but requires disassembling the speaker
  - Available CLI commands differ depending on the current firmware
- Requires a FTDI TTL-232R-3V3 serial communication device (not included)

  1. Perform the USB-C drag & drop update using the preferred firmware binary
  2. Open the MYND case and remove the speaker to expose the PCB
  3. Plug the FTDI device into a computer, install necessary drivers, and identify the tty (Linux) or COM (Windows) port that the computer has assigned to the FTDI device
  4. Connect the FTDI TX (Orange), RX (Yellow), and GND (Black) lines to the MYND's RX, TX, and GND UART pins headers, respectively (see [mynd-hardware](https://github.com/teufelaudio/mynd-hardware))
  5. **Short pins/slots 4 and 5 of the amp connector** - part P1S CJL008DH250197, otherwise the unit cannot power on (see [mynd-hardware/schematic/PDF/Mynd_AMP_CONN_SCH.PDF](https://github.com/teufelaudio/mynd-hardware/blob/main/schematic/PDF/Mynd_AMP_CONN_SCH.PDF)) 
  6. Power on the MYND speaker
  7. Open Putty (or similar application) to configure the computer's FTDI port for serial communication and set baud rate to `115200`
  8. Open the connection and send an `Enter` keystroke into the newly active terminal
  9. The `MYND$` prompt will then appear; type `help` for a list of available commands
       - To use the factory test commands, first type `AZ` to enter test mode, then `AX` to exit
  10. If experimenting with the factory firmware, reassemble and perform the USB-C drag & drop update with the production firmware binary to return the MYND speaker to normal operation

#### Method 2

- This method must be performed with the factory firmware but no disassembly required
  - UART to USB-C redirection function via the PWR+BT button combo is not **currently** included in the production firmware (see note in step 5 below)
- Requires a spliced USB-C cable and a FTDI TTL-232R-3V3 serial communication device (not included)

  1. Perform the USB-C drag & drop update using the factory firmware binary to enable the CLI functionality
  2. Obtain a junk USB-C cable and cut the cord to expose the cable's data lines (TX, RX, GND)
     - Twist together the grounded shield wires and the GND line
  3. Connect the cable's newly exposed data lines to the FTDI device's data lines, respectively
  4. Power on the MYND speaker
  5. Perform a **short-press PWR+BT button combination** to enable serial communication via USB-C
     - **Note**, this same button combo with the production firmware will instead announce the firmware versions for the MCU, BT, and PD hardware components (can help identify if the production firmware is installed); Search codebase for INCLUDE_PRODUCTION_TESTS for a better understanding
  6. Plug the FTDI device into a computer, install necessary drivers, and identify the tty (Linux) or COM (Windows) port that the computer has assigned to the FTDI device
  7. Open Putty (or similar application) to configure the computer's FTDI port for serial communication and set baud rate to `115200`.
  8. Open the connection and send an `Enter` keystroke into the newly active terminal
  9. The `MYND$` prompt will then appear; type `help` for a list of available commands
     - To use factory test commands, first type `AZ` to enter test mode, then `AX` to exit
  10. When finished experimenting, reassemble and perform the USB-C drag & drop update with the production firmware binary to return the MYND speaker to normal operation