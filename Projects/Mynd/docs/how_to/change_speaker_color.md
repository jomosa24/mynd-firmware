# How To Change Color of MYND (Shown in the Teufel Go App)

## Requirements
- An expendible USB-C cable 
- A FTDI TTL-232R-3V3 serial communication device
- Soldering kit (with pin headers, circuit board, M-F jumper wires)
- MYND factory and production update firmware binaries
   - mynd-update-firmware-mcu.bin
   - mynd-factory-update-firmware-mcu.bin
---

  1. Perform the USB-C drag & drop update using the **factory** firmware binary to enable the CLI
     - Follow the [How To guide](./perform_drag&drop_update.md)
  2. Obtain an expendible USB-C cable and cut the cord to expose the cable's data lines (TX, RX, GND)
     - Twist the grounded shield wires together with the GND line
  3. Connect the cable's newly exposed data lines to the FTDI device's data lines, respectively
     - Solder 3 pin headers to the circuit board
        - [USB-C Circuit (top side)](./images/usbc_circuit_top.png)
     - Solder the 3 exposed TX, RX, and GND wires to the bottom end of the pins (shorter side)
        - [USB-C Circuit (bottom side)](./images/usbc_circuit_bottom.png)
     - Use male to female jumper wires to connet the FTDI TX (Orange), RX (Yellow), and GND (Black) wires to pin headers RX, TX, and GND, respectively
  4. Plug the USB-C into the MYND speaker and power on
  6. Perform a **medium-press PWR+BT button combination** to enable serial communication via USB-C
  7. Plug the FTDI device into a computer
     - Install the necessary drivers
     - Identify the tty (Linux) or COM (Windows) port that the computer has assigned to the FTDI USB device
  8. Open Putty (or similar application) to configure the computer's FTDI port for serial communication and set baud rate to `115200`.
  9. Open the connection and send an `Enter` keystroke into the newly active terminal
     - The `MYND$` prompt will then appear
     - Type `help` for a list of available commands
       - [availble factory cli commands](./images/factory_cli_commands.png)
  11. Write the new color to the MYND speaker 
      - First enter `AZ` to activate test mode to use factory test commands
      - Enter either `AT00` `AT01` `AT02` `AT03` (Black, White, Wild Berry, Light Mint) to write the color
      - Enter `AU` to read the color to confirm the new color
      - Enter `AX` to deactivate test mode
      - [Read, write color example](./images/rw_color_example.png)
  12. Perform the USB-C drag & drop update using the **production** firmware binary to return the MYND speaker to normal operation
      - Follow the [How To guide](./perform_drag&drop_update.md)
 