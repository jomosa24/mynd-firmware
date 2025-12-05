# Mynd RPi Link Daemon

This daemon enables bidirectional communication between the Mynd speaker MCU and a Raspberry Pi Zero 2 W running Moode OS. It receives button events and power state changes from the MCU, executes commands (like `sudo poweroff`), and forwards button events to the Moode REST API.

**Disclaimer**: This guide is a community-driven project and does not represent an official feature. It is intended for experienced hobbyists with basic knowledge of electronics. The steps described for installing a Raspberry Pi into the MYND speaker are fully reversible and should not require any soldering. While we have not encountered any issues during our own tests, we cannot provide any warranty or accept liability for any damage that may result from following these instructions—whether to the MYND speaker itself or to external components such as the Raspberry Pi. Please check all connections carefully. Incorrectly connected cables may cause hardware damage. You undertake this modification at your own risk. We also cannot guarantee that the modification will work in every case.

## Features

- **UART Communication**: Receives messages from Mynd MCU via UART (115200 baud)
- **Power Management**: Executes `sudo poweroff` when MCU sends power-off state, with safe shutdown sequence:
  1. MCU sends `mcu:notify:state=2` (PRE_OFF) - triggers shutdown preparation
  2. RPi daemon disables periodic updates (`mcu:<id>:set send_system_snapshots 0`, `mcu:<id>:set send_button_events 0`)
  3. RPi daemon syncs filesystems for safe shutdown
  4. RPi daemon turns off LED and sends `rpi:<id>:shutdown_ready` command to MCU
  5. MCU sends `mcu:notify:state=0` (OFF) - triggers actual poweroff
  6. RPi daemon executes `sudo poweroff`
- **Button Event Forwarding**: Forwards button presses to Moode REST API for volume control and playback
- **Systemd Integration**: Runs as a systemd service with auto-restart
- **Configurable**: INI-style configuration file for customization

## Prerequisites

- Mynd Speaker
- Raspberry Pi Zero 2 W with pin headers (ideally angled), running Moode OS or similar Linux distribution
- Micro SD card with Moode OS installed and configured for i2s (Follow the How-To in the wiki)
- 8 Female-male jumper wires
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
   # Copy the daemon_installation directory to your Raspberry Pi
   scp -r daemon_install/ pi@raspberrypi.local:/home/pi/mynd_rpi_link
   ```

2. **Run installation script**:
   ```bash
   ssh pi@raspberrypi.local
   cd /home/pi/mynd_rpi_link
   sudo ./install_rpi_link.sh
   ```

The installation script will:
- Install Python dependencies (pyserial, requests)
- Copy daemon to `/usr/local/bin/`
- Install configuration to `/etc/mynd_rpi_link.conf`
- Install systemd service
- Configure sudoers (with your confirmation)
- Enable and start the service

## Configuration

Edit `/etc/mynd_rpi_link.conf` to customize:

```ini
[uart]
device = /dev/serial0          # UART device path
baudrate = 115200              # Must match MCU baud rate
timeout = 0.2                  # Read timeout in seconds

[moode]
base_url = http://localhost    # Moode REST API base URL
api_timeout = 5.0              # API request timeout
retry_count = 3                # Retry attempts for failed API calls
streaming_poll_interval = 5.0  # Interval in seconds for polling MPD streaming state
mpd_port = 6600                # MPD TCP port (used as fallback if Unix socket unavailable)

[power]
poweroff_delay = 0.0              # Not used - filesystems are synced before poweroff for immediate but safe shutdown
poweroff_command = sudo poweroff  # Command to execute (sync runs automatically before this)

[logging]
level = INFO        # Log level: DEBUG, INFO, WARNING, ERROR
use_syslog = true   # Use syslog (true) or stdout (false)
```

After changing configuration, restart the service:
```bash
sudo systemctl restart mynd-rpi-link
```

## Message Protocol

The daemon communicates with the MCU using a line-oriented text protocol over UART with request/response correlation via request IDs.

### Protocol Format

All messages follow the format: `<prefix>:<id>:<command>` or `<prefix>:notify:<key>=<value>`

- **Prefix**: `mcu` (from MCU) or `rpi` (from RPi)
- **ID**: Request ID (0-9999) for command/response correlation
- **Command**: Action and parameters
- **Notify**: Unsolicited notifications use `mcu:notify:` prefix (no ID needed)

### Messages from MCU to RPi:

#### Unsolicited Notifications (mcu:notify:)

- `mcu:notify:state=<0-4>` - Power state change
  - `0` = Off (triggers actual `sudo poweroff` execution)
  - `1` = On
  - `2` = PreOff - triggers shutdown preparation:
    - RPi daemon disables periodic updates
    - RPi daemon syncs filesystems, turns off LED, and sends `rpi:<id>:shutdown_ready` command to MCU
    - MCU then sends `state=0` to trigger actual poweroff
  - `3` = PreOn
  - `4` = Transition
- `mcu:notify:button=<bitfield>,<state>` - Button event
  - Bitfield: Power=1, BT=2, Play=4, Plus=8, Minus=16
  - State values: ShortPress=0, ShortRelease=1, MediumPress=2, MediumRelease=3, LongPress=4, LongRelease=5, VeryLongPress=6, VeryLongRelease=7, VeryVeryLongPress=8, VeryVeryLongRelease=9, DoublePress=10, DoubleRelease=11, TriplePress=12, TripleRelease=13, Hold=14
- `mcu:notify:battery=<0-100>` - Battery level percentage (periodic if enabled)
- `mcu:notify:charging=<0|1|2>` - Charging status (0=Not charging, 1=Charging, 2=Charged, periodic if enabled)
- `mcu:notify:aux=<0|1>` - Aux jack connection status (periodic if enabled)
- `mcu:notify:bfc=<0|1>` - Battery Friendly Charging state change (0=Fast Charge, 1=Battery Friendly)

#### MCU Commands to RPi (mcu:<id>:)

- `mcu:<id>:set send_button_events <0|1>` - Enable/disable button event transmission
- `mcu:<id>:set send_system_snapshots <0|1>` - Enable/disable periodic system status messages
- `mcu:<id>:set led_2 <r> <g> <b>` - Set LED 2 color (RGB 0-255)
- `mcu:<id>:set override_power <0|1>` - Override power state (for testing/debugging)
- `mcu:<id>:get streaming_active` - Get current streaming state
- `mcu:<id>:set streaming_active <0|1>` - Set streaming active state
- `mcu:<id>:get battery` - Get battery level (0-100%)
- `mcu:<id>:get charging` - Get charging status (0=Not charging, 1=Charging, 2=Charged)
- `mcu:<id>:get state` - Get power state (0=Off, 1=On)
- `mcu:<id>:get volume` - Get current volume level (0-100%)

#### MCU Responses (mcu:<id>:)

- `mcu:<id>:<key>=<value>` - Response to RPi command
- `mcu:<id>:error <message>` - Error response

### Messages from RPi to MCU:

#### RPi Commands (rpi:<id>:)

- `rpi:<id>:set volume <0-100>` - Set volume level (0-100%) 
- `rpi:<id>:get bfc` - Get Battery Friendly Charging state
- `rpi:<id>:set bfc <0|1>` - Set Battery Friendly Charging (0=Fast Charge, 1=Battery Friendly)
- `rpi:<id>:set status <0-3>` - Set RPi status (0=RPiReady, 1=RPiNotReady, 2=RPiError, 3=RPiUnknown)
- `rpi:<id>:shutdown_ready` - Notify MCU that shutdown preparation is complete (sent after PreOff state)

**Note**: 
  - The daemon does not use `rpi:<id>:get volume`, `rpi:<id>:set volume <0-100>`, `rpi:<id>:set bfc <0|1>`
    - MYND button events instead trigger queries to the Moode REST API directly to set the volume. 

#### RPi Responses (rpi:<id>:)

- `rpi:<id>:<key>=<value>` - Response to MCU command
- `rpi:<id>:error <message>` - Error response

## Button Event Handling

**CAUTION**: MYND's buttons (play, volume) do not work when streaming via Moode OS "Renderers" (Spotify, Bluetooth, Airplay, etc.), since they do not use the Moode REST API (TODO, have mcu control amp volume directly instead of using Moode's software volume control)
    - Instead, volume and playback must be controlled via the connected device in these cases

The daemon forwards button events to Moode REST API:

- **Plus button** (ShortPress=0 or Hold=14) → Volume up (increments by 5%)
- **Minus button** (ShortPress=0 or Hold=14) → Volume down (decrements by 5%)
- **Play button** (ShortPress=0) → Toggle play/pause
- **Play button** (DoublePress=10 or DoubleRelease=11) → Next radio station
- **Play button** (TriplePress=12 or TripleRelease=13) → Previous radio station

Other buttons (Power, BT) have template functions that can be extended for custom actions.

**Note**:
  - The daemon uses Moode's REST API endpoint `/command/?cmd=<command>` format. Volume commands use `set_volume -up 5` or `set_volume -dn 5` to update the web interface, and `toggle_play_pause` for play/pause control.
    - https://github.com/moode-player/docs/blob/main/setup_guide.md#61-rest-api
  - The daemon also uses the MPD protocol to detect active streaming status
    - https://mpd.readthedocs.io/en/latest/index.html

## Extending the Protocol

### Adding a New RPi Command (RPi → MCU)

To add a new command that RPi can send to MCU:

1. **Update MCU handler** (`task_rpi.cpp`):

```cpp
// In handle_message(), add to the "set" or "get" section:
if (key_equals(tokens.key, "my_new_command")) {
    int v = 0;
    if (parse_int_arg(tokens.args, v)) {
        // Process the command
        // ...
        reply_kv_with_id(tokens.id, "my_new_command", result);
    } else {
        reply_error_with_id(tokens.id, "invalid my_new_command value");
    }
    return;
}
```

2. **Update Python daemon** (`mynd_rpi_link.py`):

```python
# Add method to send the command:
def _send_my_new_command(self, value: int) -> bool:
    """Send my_new_command to MCU"""
    return self._send_to_mcu(f"set my_new_command {value}")

# Use it:
self._send_my_new_command(42)
```

### Adding a New MCU Command (MCU → RPi)

To add a new command that MCU can send to RPi:

1. **Update MCU sender** (`task_rpi.cpp`):

```cpp
// Create a function to send the command:
static void send_my_command_to_rpi(uint32_t id, int value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "mcu:%u:set my_command %d\n", id, value);
    uart_write(buf);
}
```

2. **Update Python daemon** (`mynd_rpi_link.py`):

```python
# In _process_message(), add handling:
elif msg_type == 'response':
    key = msg['key']
    if key == 'my_command':
        value = msg['value']
        # Process the command
        self._handle_my_command(value)
```

### Adding a New Notification Type

To add a new unsolicited notification from MCU:

1. **Update MCU sender** (`task_rpi.cpp`):

```cpp
static void send_my_notification(int value) {
    char buf[48];
    snprintf(buf, sizeof(buf), "mcu:notify:my_notification=%d\n", value);
    uart_write(buf);
}
```

2. **Update Python daemon** (`mynd_rpi_link.py`):

```python
# In _process_message(), add to 'notify' section:
elif msg_type == 'notify':
    key = msg['key']
    if key == 'my_notification':
        value = msg['value']
        self._handle_my_notification(value)
```

### Tokenizer Usage Example

The MCU uses a tokenizer to parse commands. The tokenizer uses a consistent approach where prefixes, actions, and keys are tokenized into constants for efficient and safe pointer comparison:

```cpp
// Tokenized constants (defined at top of file)
static const char *const PREFIX_RPI = "rpi";
static const char *const ACTION_SET = "set";
static const char *const KEY_VOLUME = "volume";

// Parse a command line
CommandTokens tokens = tokenize_command("rpi:42:set volume 75");

// tokens.prefix points to PREFIX_RPI constant (pointer comparison)
// tokens.id = 42
// tokens.action points to ACTION_SET constant (pointer comparison)
// tokens.key points to KEY_VOLUME constant (pointer comparison)
// tokens.args = "75" (pointer to input string)
// tokens.valid = true

// Use pointer comparison for tokenized values (fast, no strcmp needed)
if (tokens.prefix == PREFIX_RPI && tokens.action == ACTION_SET && tokens.key == KEY_VOLUME) {
    // Use helper functions to parse arguments
    int volume = 0;
    if (parse_int_arg(tokens.args, volume)) {
        // volume = 75
    }
}
```

**Tokenization Benefits:**
- **Efficient**: Pointer comparison instead of string comparison
- **Type-safe**: Compiler can catch typos in constant names
- **Consistent**: All prefixes, actions, and keys use the same pattern
- **No warnings**: Avoids "comparison against string literal" compiler warnings

### Command Handler Template

Here's a complete template for adding a new command handler:

```cpp
// 1. Add key constant at top of file with other tokenized constants:
static const char *const KEY_MY_COMMAND = "my_command";

// 2. Update tokenization to recognize the new key:
// In tokenize_command(), add to the key matching section:
else if (strncmp(key_start, "my_command", key_len) == 0 && key_len == 10)
    tokens.key = KEY_MY_COMMAND;

// 3. In handle_message(), use pointer comparison:
if (tokens.action == ACTION_SET) {
    if (tokens.key == KEY_MY_COMMAND) {
        int v = 0;
        if (parse_int_arg(tokens.args, v)) {
            v = clamp(v, 0, 100);  // Validate range
            // Process command
            // ...
            reply_kv_with_id(tokens.id, "my_command", v);
        } else {
            reply_error_with_id(tokens.id, "invalid my_command value");
        }
        return;
    }
}
```

### Adding Custom Button Actions

Edit `mynd_rpi_link.py` and modify the `_handle_button_event()` method:

```python
def _handle_button_event(self, button_bitfield: int, input_state: int):
    # ... existing code ...
    
    # Add custom handling
    if button_bitfield & BUTTON_POWER:
        if input_state == INPUT_STATE_LONG_PRESS:
            # Custom action for power button long press
            self._custom_power_action()
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

### Moode REST API not responding

1. Verify Moode is running:
   ```bash
   # Test Moode REST API using the command endpoint (same format as daemon uses)
   curl "http://localhost/command/?cmd=get_volume"
   # Should return JSON like: {"volume": "50"}
   ```

2. Check Moode REST API base URL in configuration (default: `http://localhost`)

3. Verify network connectivity

4. Check Moode logs if API calls are failing:
   ```bash
   # Moode logs are typically in /var/log/moode.log or via journalctl
   sudo journalctl -u moode -f
   ```

## Manual Testing

Run the daemon in foreground mode for debugging:

```bash
sudo /usr/local/bin/mynd_rpi_link.py --config /etc/mynd_rpi_link.conf --foreground
```

### Testing UART Communication

**Note**: The daemon must be stopped before manually testing UART communication, as only one process can access the serial port at a time.

1. **Stop the daemon**:
   ```bash
   sudo systemctl stop mynd-rpi-link
   ```

2. **Send test commands to MCU** (using new protocol format):
   ```bash
   # Method 1: Using echo with explicit newline
   echo -e "rpi:1:get battery\n" > /dev/serial0
   
   # Method 2: Using printf (more reliable)
   printf "rpi:1:get battery\n" > /dev/serial0
   printf "rpi:2:set send_button_events 1\n" > /dev/serial0
   printf "rpi:3:set send_system_snapshots 1\n" > /dev/serial0
   printf "rpi:4:get bfc\n" > /dev/serial0
   ```

3. **Monitor MCU responses** (if you have access to MCU debug output):
   - Check MCU logs for received commands
   - MCU should respond with messages like `rpi:1:battery=85`

4. **Test bidirectional communication**:
   ```bash
   # Send command and read response (requires cat or screen)
   printf "rpi:1:get battery\n" > /dev/serial0
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
1. Save the original volume using `rpi:<id>:get volume` before starting tests
2. Restore the original volume at the end of testing using `rpi:<id>:set volume <original_value>`
3. This ensures the device returns to its previous volume level after testing

You can create a simple test script using Python's `pyserial` library:

```python
import serial
import time

# Open serial connection
ser = serial.Serial('/dev/serial0', 115200, timeout=1)
time.sleep(0.2)
ser.reset_input_buffer()

request_id = 1

# Disable periodic updates to prevent interference
ser.write(f"rpi:{request_id}:set send_system_snapshots 0\n".encode())
request_id += 1
time.sleep(0.1)
ser.write(f"rpi:{request_id}:set send_button_events 0\n".encode())
request_id += 1
time.sleep(0.1)
ser.reset_input_buffer()

# Test commands
ser.write(f"rpi:{request_id}:get battery\n".encode())
request_id += 1
time.sleep(0.2)
response = ser.readline()
print(f"Battery response: {response.decode().strip()}")

# Test BFC
ser.write(f"rpi:{request_id}:get bfc\n".encode())
request_id += 1
time.sleep(0.2)
response = ser.readline()
print(f"BFC response: {response.decode().strip()}")

# Remember to restore periodic updates if needed
ser.close()
```

For comprehensive testing, create a script that:
- Disables periodic updates at the start
- Tests GET commands (battery, charging, state, volume, bfc, etc.)
- Tests SET commands (volume, bfc, system flags, etc.)
- Re-enables periodic updates at the end
- Uses request IDs for proper correlation

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
sudo rm /usr/local/bin/mynd_rpi_link.py
sudo rm /etc/mynd_rpi_link.conf
sudo rm /etc/sudoers.d/mynd-rpi-link

# Reload systemd
sudo systemctl daemon-reload
```

## Support

For issues or questions, refer to the main project documentation or contact the development team.