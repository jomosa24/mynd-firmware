# How To Use the MYND Command-Line Interface (CLI)

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
  5. Perform a **medium-press PWR+BT button combination** to enable serial communication via USB-C
     - **Note**, this same button combo with the production firmware will instead announce the firmware versions for the MCU, BT, and PD hardware components (can help identify if the production firmware is installed); Search codebase for INCLUDE_PRODUCTION_TESTS for a better understanding
  6. Plug the FTDI device into a computer, install necessary drivers, and identify the tty (Linux) or COM (Windows) port that the computer has assigned to the FTDI device
  7. Open Putty (or similar application) to configure the computer's FTDI port for serial communication and set baud rate to `115200`.
  8. Open the connection and send an `Enter` keystroke into the newly active terminal
  9. The `MYND$` prompt will then appear; type `help` for a list of available commands
     - To use factory test commands, first type `AZ` to enter test mode, then `AX` to exit
  10. When finished experimenting, reassemble and perform the USB-C drag & drop update with the production firmware binary to return the MYND speaker to normal operation