# Mynd RPi Link Daemon

This daemon enables bidirectional communication between the Mynd speaker MCU and a Raspberry Pi Zero W running Moode OS. It receives button events and power state changes from the MCU, executes commands (like `sudo poweroff`), and forwards button events to the Moode API.

## Features

- **UART Communication**: Receives messages from Mynd MCU via UART (115200 baud)
- **Power Management**: Executes `sudo poweroff` when MCU sends power-off state, with safe shutdown sequence:
  1. MCU sends `state=0` (power off request)
  2. RPi daemon syncs filesystems for safe shutdown
  3. RPi daemon sends `shutdown_ready=1` confirmation to MCU
  4. RPi daemon executes `sudo poweroff`
- **Button Event Forwarding**: Forwards button presses to Moode API for volume control and playback
- **Systemd Integration**: Runs as a systemd service with auto-restart
- **Configurable**: JSON-style configuration file for customization

## Prerequisites

- Raspberry Pi Zero W (or compatible) running Moode OS or similar Linux distribution
- Python 3.6 or higher
- UART connection between MCU and Raspberry Pi (typically `/dev/serial0`)
- Root/sudo access for installation

## Prerequisites Setup

### Enable Serial Port

**IMPORTANT**: The serial port must be enabled before `/dev/serial0` will be available.

1. **Enable serial port via raspi-config**:
   ```bash
   sudo raspi-config
   ```

2. Navigate to:
   - **Interface Options** → **Serial Port**
   - Select **No** to disable serial login shell (if prompted)
   - Select **Yes** to enable serial port hardware

3. **Reboot the Raspberry Pi**:
   ```bash
   sudo reboot
   ```

4. **Verify serial port is available**:
   ```bash
   ls -l /dev/serial*
   # Should show /dev/serial0 -> ttyS0
   ```

   If the device is not found, check:
   ```bash
   ls -l /dev/ttyS0
   # Should show the device file
   ```

### Alternative: Enable via config.txt (if raspi-config is not available)

If `raspi-config` is not available, you can enable the serial port manually:

1. Edit `/boot/config.txt`:
   ```bash
   sudo nano /boot/config.txt
   ```

2. Add or uncomment:
   ```
   enable_uart=1
   ```

3. Reboot:
   ```bash
   sudo reboot
   ```

## Installation

1. **Copy files to Raspberry Pi**:
   ```bash
   # Copy the tools directory to your Raspberry Pi
   scp -r tools/ pi@raspberrypi.local:/home/pi/mynd_rpi_daemon
   ```

2. **Run installation script**:
   ```bash
   ssh pi@raspberrypi.local
   cd /home/pi/mynd_rpi_daemon
   sudo ./install_rpi_daemon.sh
   ```

The installation script will:
- Install Python dependencies (pyserial, requests)
- Copy daemon to `/usr/local/bin/`
- Install configuration to `/etc/mynd_rpi_daemon.conf`
- Install systemd service
- Configure sudoers (with your confirmation)
- Enable and start the service

## Configuration

Edit `/etc/mynd_rpi_daemon.conf` to customize:

```ini
[uart]
device = /dev/serial0      # UART device path
baudrate = 115200          # Must match MCU baud rate
timeout = 0.2              # Read timeout in seconds

[moode]
base_url = http://localhost # Moode API base URL
api_timeout = 5.0           # API request timeout
retry_count = 3            # Retry attempts for failed API calls

[power]
poweroff_delay = 0.0       # Not used - filesystems are synced before poweroff for immediate but safe shutdown
poweroff_command = sudo poweroff  # Command to execute (sync runs automatically before this)
# Note: poweroff_delay is ignored - the daemon always syncs filesystems before poweroff for safety

[logging]
level = INFO               # Log level: DEBUG, INFO, WARNING, ERROR
use_syslog = true          # Use syslog (true) or stdout (false)
```

After changing configuration, restart the service:
```bash
sudo systemctl restart mynd-rpi-link
```

## Message Protocol

The daemon communicates with the MCU using a line-oriented text protocol over UART.

### Messages from MCU to RPi:

- `state=0` - Power off state (triggers safe shutdown sequence: filesystem sync → `shutdown_ready=1` → `sudo poweroff`)
- `state=1` - Power on state
- `button=<bitfield>,<state>` - Button event
  - Bitfield: Power=1, BT=2, Play=4, Plus=8, Minus=16
  - State values:
    - ShortPress=0, ShortRelease=1
    - MediumPress=2, MediumRelease=3
    - LongPress=4, LongRelease=5
    - VeryLongPress=6, VeryLongRelease=7
    - VeryVeryLongPress=8, VeryVeryLongRelease=9
    - DoublePress=10, DoubleRelease=11
    - TriplePress=12, TripleRelease=13
    - Hold=14
- `battery=<0-100>` - Battery level percentage
- `charging=<0|1|2>` - Charging status (0=Not charging, 1=Charging, 2=Charged)
- `aux=<0|1>` - Aux jack connection status

### Messages from RPi to MCU:

- `set volume <0-100>` - Set volume level (0-100%)
- `get volume` - Get current volume (returns 0-100)
- `set send_buttons <0|1>` - Enable/disable button event transmission
- `set send_system <0|1>` - Enable/disable periodic system status messages (battery, charging, state, aux)
- `set override_power <0|1>` - Enable/disable power override (required for `set led_1`)
- `set override_volume <0|1>` - Enable/disable volume override (placeholder, currently reuses send_buttons flag)
- `set power_on <0|1>` - Power on device (1) or no-op (0)
- `set power_off <seconds>` - Schedule power off (not implemented, returns value but does nothing)
- `set ecomode <0|1>` - Set eco mode (not fully implemented)
- `get ecomode` - Get eco mode (always returns 0)
- `set bfc <0|1>` - Set bass/treble control (not fully implemented)
- `get bfc` - Get bass/treble control (always returns 0)
- `set led_1 <r> <g> <b>` - Set LED 1 color (requires override_power=1, RGB 0-255)
- `set led_2 <r> <g> <b>` - Set LED 2 color (RGB 0-255)
- `get battery` - Get battery level (0-100)
- `get charging` - Get charging status (0=Not charging, 1=Charging, 2=Charged)
- `get state` - Get power state (0=Off, 1=On)
- `get power` - Get power value (always returns 0)
- `shutdown_ready=1` - Sent by RPi daemon to MCU when shutdown is ready (after filesystem sync, before poweroff)

## Button Event Handling

The daemon forwards button events to Moode API:

- **Plus button** (ShortPress=0 or Hold=14) → Volume up (increments by 5%)
- **Minus button** (ShortPress=0 or Hold=14) → Volume down (decrements by 5%)
- **Play button** (ShortPress=0) → Toggle play/pause
- **Play button** (DoublePress=10 or DoubleRelease=11) → Next radio station
- **Play button** (TriplePress=12 or TripleRelease=13) → Previous radio station

Other buttons (Power, BT) have template functions that can be extended for custom actions.

**Note**: The daemon uses Moode's REST API endpoint `/command/?cmd=<command>` format. Volume commands use `set_volume -up 5` or `set_volume -dn 5` to update the web interface, and `toggle_play_pause` for play/pause control.

## Extending the Daemon

### Adding Custom Button Actions

Edit `mynd_rpi_daemon.py` and modify the `_handle_button_event()` method:

```python
def _handle_button_event(self, button_bitfield: int, input_state: int):
    # ... existing code ...
    
    # Add custom handling
    if button_bitfield & BUTTON_POWER:
        if input_state == INPUT_STATE_LONG_PRESS:
            # Custom action for power button long press
            self._custom_power_action()
```

### Adding Custom Commands

Add new command execution templates in the daemon:

```python
def _execute_custom_command(self, command: str):
    """Template for executing custom commands"""
    try:
        result = subprocess.run(
            command.split(),
            capture_output=True,
            text=True,
            timeout=10
        )
        if result.returncode == 0:
            self.logger.info(f"Command executed: {command}")
        else:
            self.logger.error(f"Command failed: {result.stderr}")
    except Exception as e:
        self.logger.error(f"Error executing command: {e}")
```

## Troubleshooting

### Service not starting

Check service status:
```bash
sudo systemctl status mynd-rpi-link
```

View logs:
```bash
sudo journalctl -u mynd-rpi-link -f
```

### UART connection issues

1. **Verify UART is enabled** (if not done during prerequisites):
   ```bash
   sudo raspi-config
   # Navigate to: Interface Options → Serial Port → Enable
   # Reboot after enabling
   ```

2. **Check UART device**:
   ```bash
   ls -l /dev/serial*
   # Should show /dev/serial0 -> ttyS0
   
   # If serial0 is not found, check ttyS0 directly:
   ls -l /dev/ttyS0
   ```

3. **Check if UART is enabled in config.txt**:
   ```bash
   grep enable_uart /boot/config.txt
   # Should show: enable_uart=1
   ```

4. **Test UART manually**:
   ```bash
   sudo python3 -c "import serial; s=serial.Serial('/dev/serial0', 115200); print('UART OK')"
   ```

5. **If /dev/serial0 is not found**:
   - Make sure you've enabled the serial port via `raspi-config` and rebooted
   - Try using `/dev/ttyS0` directly (update config file)
   - Check that the serial port is not being used by another service (e.g., Bluetooth)

### Poweroff not working

1. Verify sudoers configuration:
   ```bash
   sudo visudo -c -f /etc/sudoers.d/mynd-rpi-link
   ```

2. Test poweroff command manually:
   ```bash
   sudo poweroff
   ```

3. Check daemon logs for errors

### Moode API not responding

1. Verify Moode is running:
   ```bash
   # Test Moode API using the command endpoint (same format as daemon uses)
   curl "http://localhost/command/?cmd=get_volume"
   # Should return JSON like: {"volume": "50"}
   ```

2. Check Moode API base URL in configuration (default: `http://localhost`)

3. Verify network connectivity

4. Check Moode logs if API calls are failing:
   ```bash
   # Moode logs are typically in /var/log/moode.log or via journalctl
   sudo journalctl -u moode -f
   ```

## Manual Testing

Run the daemon in foreground mode for debugging:

```bash
sudo /usr/local/bin/mynd_rpi_daemon.py --config /etc/mynd_rpi_daemon.conf --foreground
```

### Testing UART Communication

**Note**: The daemon must be stopped before manually testing UART communication, as only one process can access the serial port at a time.

1. **Stop the daemon**:
   ```bash
   sudo systemctl stop mynd-rpi-link
   ```

2. **Send test commands to MCU**:
   ```bash
   # Method 1: Using echo with explicit newline
   echo -e "get battery\n" > /dev/serial0
   
   # Method 2: Using printf (more reliable)
   printf "get battery\n" > /dev/serial0
   printf "set send_buttons 1\n" > /dev/serial0
   printf "set send_system 1\n" > /dev/serial0
   ```

3. **Monitor MCU responses** (if you have access to MCU debug output):
   - Check MCU logs for received commands
   - MCU should respond with messages like `battery=85`

4. **Test bidirectional communication**:
   ```bash
   # Send command and read response (requires cat or screen)
   printf "get battery\n" > /dev/serial0
   cat < /dev/serial0
   # Or use screen/minicom for interactive testing:
   sudo screen /dev/serial0 115200
   # Press Ctrl+A then K to exit screen
   ```

5. **Restart the daemon**:
   ```bash
   sudo systemctl start mynd-rpi-link
   ```

### Testing with Python Script

**Note for test scripts**: When testing volume commands, test scripts should:
1. Save the original volume using `get volume` before starting tests
2. Restore the original volume at the end of testing using `set volume <original_value>`
3. This ensures the device returns to its previous volume level after testing

You can also use the included test script (if available) or create a comprehensive test:

```python
#!/usr/bin/env python3
"""
Comprehensive test script for Mynd RPi Link protocol
Tests all available GET and SET commands
"""
import serial
import time
import sys

def send_command(ser, command, expected_key=None, timeout=0.3, read_all=False):
    """Send a command and read the response
    
    Args:
        ser: Serial port object
        command: Command string to send
        expected_key: Expected response key (e.g., "battery", "volume")
        timeout: Time to wait for response in seconds
        read_all: If True, read all available responses and filter for expected_key
    """
    # Clear any pending input
    ser.reset_input_buffer()
    
    # Send command
    if not command.endswith('\n'):
        command += '\n'
    ser.write(command.encode())
    ser.flush()
    
    # Wait for response
    time.sleep(timeout)
    
    # Read response(s)
    responses = []
    if read_all:
        # Read all available responses for up to 300ms
        start_time = time.time()
        while time.time() - start_time < 0.3:
            if ser.in_waiting > 0:
                response = ser.readline()
                if response:
                    response_str = response.decode('utf-8', errors='replace').strip()
                    responses.append(response_str)
            else:
                time.sleep(0.01)
    else:
        # Read single response
        response = ser.readline()
        if response:
            response_str = response.decode('utf-8', errors='replace').strip()
            responses.append(response_str)
    
    # Filter for expected response if key is provided
    if expected_key and responses:
        matching = [r for r in responses if r.startswith(expected_key + '=')]
        if matching:
            response_str = matching[0]
            print(f"  Command: {command.strip():30} Response: {response_str}")
            return response_str
        elif responses:
            # Show all responses if none match
            response_str = responses[0]
            print(f"  Command: {command.strip():30} Response: {response_str}")
            print(f"    WARNING: Expected response starting with '{expected_key}=', got '{response_str}'")
            if len(responses) > 1:
                print(f"    (Also received: {', '.join(responses[1:])})")
            return response_str
    
    # No expected key or no matching response
    if responses:
        response_str = responses[0]
        print(f"  Command: {command.strip():30} Response: {response_str}")
        if len(responses) > 1:
            print(f"    (Also received: {', '.join(responses[1:])})")
        return response_str
    else:
        print(f"  Command: {command.strip():30} Response: <NO RESPONSE>")
        return None

def main():
    # Open serial connection
    try:
        ser = serial.Serial('/dev/serial0', 115200, timeout=1)
        time.sleep(0.2)  # Wait for UART to settle
        ser.reset_input_buffer()
        ser.reset_output_buffer()
    except Exception as e:
        print(f"ERROR: Failed to open serial port: {e}", file=sys.stderr)
        print("Make sure the daemon is stopped: sudo systemctl stop mynd-rpi-link", file=sys.stderr)
        return 1
    
    print("=" * 70)
    print("Mynd RPi Link Protocol Test")
    print("=" * 70)
    print()
    
    try:
        # Disable periodic updates to prevent interference
        print("Initialization - Disabling periodic updates:")
        print("-" * 70)
        send_command(ser, "set send_system 0", "send_system", timeout=0.2)
        send_command(ser, "set send_buttons 0", "send_buttons", timeout=0.2)
        time.sleep(0.1)  # Let any pending messages clear
        ser.reset_input_buffer()
        print()
        # Test GET commands
        print("GET Commands:")
        print("-" * 70)
        send_command(ser, "get battery", "battery")
        send_command(ser, "get charging", "charging")
        send_command(ser, "get state", "state")
        send_command(ser, "get volume", "volume")
        send_command(ser, "get ecomode", "ecomode")
        send_command(ser, "get bfc", "bfc")
        send_command(ser, "get power", "power")
        print()
        
        # Test SET commands - Volume
        print("SET Commands - Volume:")
        print("-" * 70)
        send_command(ser, "set volume 0", "volume")
        send_command(ser, "get volume", "volume")
        send_command(ser, "set volume 50", "volume")
        send_command(ser, "get volume", "volume")
        send_command(ser, "set volume 100", "volume")
        send_command(ser, "get volume", "volume")
        send_command(ser, "set volume 75", "volume")  # Restore to reasonable level
        print()
        
        # Test SET commands - System flags
        print("SET Commands - System Flags:")
        print("-" * 70)
        send_command(ser, "set send_buttons 1", "send_buttons", timeout=0.25)
        send_command(ser, "set send_buttons 0", "send_buttons", timeout=0.25)
        send_command(ser, "set send_system 1", "send_system", timeout=0.25)
        send_command(ser, "set send_system 0", "send_system", timeout=0.25)
        send_command(ser, "set override_volume 1", "override_volume", timeout=0.25)
        send_command(ser, "set override_volume 0", "override_volume", timeout=0.25)
        send_command(ser, "set override_power 1", "override_power", timeout=0.25)
        send_command(ser, "set override_power 0", "override_power", timeout=0.25)
        print()
        
        # Test SET commands - Audio settings
        print("SET Commands - Audio Settings:")
        print("-" * 70)
        print("  Note: ecomode and bfc settings may not persist (not fully implemented)")
        send_command(ser, "set ecomode 1", "ecomode", timeout=0.25)
        send_command(ser, "get ecomode", "ecomode", timeout=0.25)
        send_command(ser, "set ecomode 0", "ecomode", timeout=0.25)
        send_command(ser, "get ecomode", "ecomode", timeout=0.25)
        send_command(ser, "set bfc 1", "bfc", timeout=0.25)
        send_command(ser, "get bfc", "bfc", timeout=0.25)
        send_command(ser, "set bfc 0", "bfc", timeout=0.25)
        send_command(ser, "get bfc", "bfc", timeout=0.25)
        print()
        
        # Test SET commands - LEDs (requires override_power for led_1)
        print("SET Commands - LEDs:")
        print("-" * 70)
        print("  Note: Disabling send_system to prevent interference with LED responses")
        send_command(ser, "set send_system 0", "send_system", timeout=0.2)
        time.sleep(0.1)
        ser.reset_input_buffer()
        send_command(ser, "set override_power 1", "override_power", timeout=0.25)
        send_command(ser, "set led_1 255 0 0", "led_1", timeout=0.3, read_all=True)  # Red
        time.sleep(0.2)
        send_command(ser, "set led_1 0 255 0", "led_1", timeout=0.3, read_all=True)  # Green
        time.sleep(0.2)
        send_command(ser, "set led_1 0 0 255", "led_1", timeout=0.3, read_all=True)  # Blue
        time.sleep(0.2)
        send_command(ser, "set led_1 0 0 0", "led_1", timeout=0.3, read_all=True)   # Off
        send_command(ser, "set led_2 255 255 255", "led_2", timeout=0.3, read_all=True)  # White
        time.sleep(0.2)
        send_command(ser, "set led_2 128 128 128", "led_2", timeout=0.3, read_all=True)  # Gray
        time.sleep(0.2)
        send_command(ser, "set led_2 0 0 0", "led_2", timeout=0.3, read_all=True)   # Off
        send_command(ser, "set override_power 0", "override_power", timeout=0.25)
        print()
        
        # Test SET commands - Power (note: power_on may trigger actual power-on)
        print("SET Commands - Power:")
        print("-" * 70)
        print("  Note: set power_on 1 may trigger actual power-on, skipping...")
        # send_command(ser, "set power_on 1")  # Uncomment to test (may power on device)
        send_command(ser, "set power_off 5", None)  # No response expected (not implemented)
        print()
        
        # Final status check
        print("Final Status Check:")
        print("-" * 70)
        send_command(ser, "get battery", "battery", timeout=0.25)
        send_command(ser, "get charging", "charging", timeout=0.25)
        send_command(ser, "get state", "state", timeout=0.25)
        send_command(ser, "get volume", "volume", timeout=0.25)
        print()
        
        # Cleanup - ensure periodic updates are disabled
        print("Cleanup - Ensuring periodic updates are disabled:")
        print("-" * 70)
        send_command(ser, "set send_system 0", "send_system", timeout=0.2)
        send_command(ser, "set send_buttons 0", "send_buttons", timeout=0.2)
        print()
        
        print("=" * 70)
        print("Test completed!")
        print("=" * 70)
        
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        print("Attempting to disable periodic updates...")
        try:
            ser.reset_input_buffer()
            ser.write(b"set send_system 0\n")
            ser.flush()
            time.sleep(0.1)
            ser.write(b"set send_buttons 0\n")
            ser.flush()
            time.sleep(0.1)
        except:
            pass
        return 1
    except Exception as e:
        print(f"\n\nERROR: {e}", file=sys.stderr)
        return 1
    finally:
        # Always try to disable periodic updates before closing
        try:
            ser.reset_input_buffer()
            ser.write(b"set send_system 0\n")
            ser.flush()
            time.sleep(0.1)
        except:
            pass
        ser.close()
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
```

## Service Management

```bash
# Start service
sudo systemctl start mynd-rpi-link

# Stop service
sudo systemctl stop mynd-rpi-link

# Restart service
sudo systemctl restart mynd-rpi-link

# Enable on boot
sudo systemctl enable mynd-rpi-link

# Disable on boot
sudo systemctl disable mynd-rpi-link

# View logs
sudo journalctl -u mynd-rpi-link -f

# Check status
sudo systemctl status mynd-rpi-link
```

## Security Notes

- The sudoers configuration allows passwordless `poweroff` execution
- Only grant this permission if you trust the MCU communication
- The daemon runs as a non-root user (default: `pi`)
- UART communication is not encrypted - use only on trusted networks

## Uninstallation

To remove the daemon:

```bash
# If the service is masked, unmask it first
sudo systemctl unmask mynd-rpi-link

# Stop and disable the service
sudo systemctl stop mynd-rpi-link
sudo systemctl disable mynd-rpi-link

# Remove the files
sudo rm /etc/systemd/system/mynd-rpi-link.service
sudo rm /usr/local/bin/mynd_rpi_daemon.py
sudo rm /etc/mynd_rpi_daemon.conf
sudo rm /etc/sudoers.d/mynd-rpi-link

# Reload systemd
sudo systemctl daemon-reload
```

## Support

For issues or questions, refer to the main project documentation or contact the development team.

