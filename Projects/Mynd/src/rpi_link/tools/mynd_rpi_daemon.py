#!/usr/bin/env python3
"""
Mynd RPi Link Daemon

Receives messages from Mynd MCU via UART and executes commands on the Raspberry Pi.
Forwards button events to Moode API and handles power state changes.
"""

import argparse
import configparser
import json
import logging
import logging.handlers
import os
import signal
import subprocess
import sys
import time
import threading
from pathlib import Path
from typing import Optional, Dict, Any
from urllib.parse import quote

try:
    import serial  # pyserial
except ImportError:
    print("ERROR: pyserial is required. Install with: pip3 install pyserial", file=sys.stderr)
    sys.exit(1)

try:
    import requests  # requests library for HTTP
except ImportError:
    print("ERROR: requests is required. Install with: pip3 install requests", file=sys.stderr)
    sys.exit(1)


# Button bitfield constants
BUTTON_POWER = 1
BUTTON_BT = 2
BUTTON_PLAY = 4
BUTTON_PLUS = 8
BUTTON_MINUS = 16

# InputState enum values (from MCU)
INPUT_STATE_SHORT_PRESS = 0
INPUT_STATE_SHORT_RELEASE = 1
INPUT_STATE_MEDIUM_PRESS = 2
INPUT_STATE_MEDIUM_RELEASE = 3
INPUT_STATE_LONG_PRESS = 4
INPUT_STATE_LONG_RELEASE = 5
INPUT_STATE_VERY_LONG_PRESS = 6
INPUT_STATE_VERY_LONG_RELEASE = 7
INPUT_STATE_VERY_VERY_LONG_PRESS = 8
INPUT_STATE_VERY_VERY_LONG_RELEASE = 9
INPUT_STATE_DOUBLE_PRESS = 10
INPUT_STATE_DOUBLE_RELEASE = 11
INPUT_STATE_TRIPLE_PRESS = 12
INPUT_STATE_TRIPLE_RELEASE = 13
INPUT_STATE_HOLD = 14


class MyndRpiDaemon:
    """Main daemon class for Mynd RPi Link"""
    
    def __init__(self, config_path: str):
        self.config = self._load_config(config_path)
        self.running = False
        self.ser: Optional[serial.Serial] = None
        self.logger = self._setup_logging()
        self.poweroff_scheduled = False
        
    def _load_config(self, config_path: str) -> Dict[str, Any]:
        """Load configuration from file"""
        config = configparser.ConfigParser()
        config.read(config_path)
        
        defaults = {
            'uart': {
                'device': '/dev/serial0',
                'baudrate': '115200',
                'timeout': '0.2'
            },
            'moode': {
                'base_url': 'http://localhost',
                'api_timeout': '5.0',
                'retry_count': '3'
            },
            'power': {
                'poweroff_delay': '0.0',  # No delay - sync happens before poweroff for safety
                'poweroff_command': 'sudo poweroff'
            },
            'logging': {
                'level': 'INFO',
                'use_syslog': 'true'
            }
        }
        
        # Merge defaults with config file
        result = {}
        for section, values in defaults.items():
            result[section] = {}
            if config.has_section(section):
                for key, default_value in values.items():
                    result[section][key] = config.get(section, key, fallback=default_value)
            else:
                result[section] = values.copy()
        
        return result
    
    def _setup_logging(self) -> logging.Logger:
        """Setup logging to syslog or stdout"""
        logger = logging.getLogger('mynd_rpi_daemon')
        
        use_syslog = self.config['logging'].get('use_syslog', 'true').lower() == 'true'
        level_str = self.config['logging'].get('level', 'INFO').upper()
        level = getattr(logging, level_str, logging.INFO)
        
        logger.setLevel(level)
        
        stdout_formatter = logging.Formatter('%(asctime)s [%(levelname)s] %(message)s')
        
        if use_syslog:
            # Use syslog handler
            try:
                handler = logging.handlers.SysLogHandler(address='/dev/log')
                handler.setFormatter(logging.Formatter('mynd_rpi_daemon[%(process)d]: %(message)s'))
            except Exception:
                # Fallback to stdout if syslog fails
                handler = logging.StreamHandler(sys.stdout)
                handler.setFormatter(stdout_formatter)
        else:
            handler = logging.StreamHandler(sys.stdout)
            handler.setFormatter(stdout_formatter)
        
        logger.addHandler(handler)
        return logger
    
    def _open_uart(self) -> bool:
        """Open UART connection"""
        try:
            device = self.config['uart']['device']
            baudrate = int(self.config['uart']['baudrate'])
            timeout = float(self.config['uart']['timeout'])
            
            self.ser = serial.Serial(
                port=device,
                baudrate=baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=timeout,
                write_timeout=timeout
            )
            
            # Give UART time to settle
            time.sleep(0.1)
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            
            self.logger.info(f"Opened UART: {device} @ {baudrate} baud")
            return True
            
        except Exception as e:
            self.logger.error(f"Failed to open UART: {e}")
            return False
    
    def _close_uart(self):
        """Close UART connection"""
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
                self.logger.info("Closed UART connection")
            except Exception as e:
                self.logger.error(f"Error closing UART: {e}")
    
    def _turn_off_led(self):
        """Turn off LED 2"""
        try:
            if self._send_command("set led_2 0 0 0"):
                self.logger.info("Turned off LED 2")
            else:
                self.logger.warning("Failed to turn off LED 2")
            time.sleep(0.1)
        except Exception as e:
            self.logger.error(f"Error turning off LED: {e}")
    
    def _send_to_mcu(self, data: str) -> bool:
        """Send command or message to MCU"""
        if not self.ser or not self.ser.is_open:
            return False
        
        try:
            if not data.endswith('\n'):
                data += '\n'
            self.ser.write(data.encode())
            self.ser.flush()
            self.logger.debug(f"Sent to MCU: {data.strip()}")
            return True
        except Exception as e:
            self.logger.error(f"Failed to send to MCU: {e}")
            return False
    
    def _send_command(self, command: str) -> bool:
        """Send command to MCU (alias for _send_to_mcu)"""
        return self._send_to_mcu(command)
    
    def _send_message(self, message: str) -> bool:
        """Send message to MCU (alias for _send_to_mcu)"""
        return self._send_to_mcu(message)
    
    def _initialize_mcu(self):
        """Initialize MCU with startup commands"""
        try:
            self.logger.info("Initializing MCU: enabling all periodic updates and setting LED 2 to Raspberry Pi color")
            
            # Enable periodic system updates (battery, charging, state, aux)
            if self._send_command("set send_system 1"):
                self.logger.info("Enabled periodic system updates")
            else:
                self.logger.warning("Failed to enable periodic system updates")
            time.sleep(0.1)
            
            # Enable button event forwarding
            if self._send_command("set send_buttons 1"):
                self.logger.info("Enabled button event forwarding")
            else:
                self.logger.warning("Failed to enable button event forwarding")
            time.sleep(0.1)
            
            # Set LED 2 to Raspberry Pi color (official Raspberry Pi red: RGB 227, 27, 27)
            # Note: LED 2 doesn't require override_power (only LED 1 does)
            if self._send_command("set led_2 227 27 27"):
                self.logger.info("Set LED 2 to Raspberry Pi color (227, 27, 27)")
            else:
                self.logger.warning("Failed to set LED 2 color")
            time.sleep(0.1)
            
            self.logger.info("MCU initialization complete")
            
            # Check Moode volume and set to 5% if it's 0% for audible ready chime
            current_volume = self._moode_get_volume()
            if current_volume is not None:
                if current_volume == 0:
                    self.logger.info("Moode volume is 0%, setting to 5%")
                    if not self._moode_set_volume(5):
                        self.logger.warning("Failed to set Moode volume to 5%")
            else:
                self.logger.warning("Failed to get Moode volume, skipping volume check")
        except Exception as e:
            self.logger.error(f"Error during MCU initialization: {e}", exc_info=True)
    
    def _parse_message(self, line: str) -> Optional[Dict[str, Any]]:
        """Parse message from MCU"""
        line = line.strip()
        if not line:
            return None
        
        # Format: key=value or key=value1,value2
        if '=' not in line:
            return None
        
        key, value = line.split('=', 1)
        
        # Handle comma-separated values
        if ',' in value:
            values = [v.strip() for v in value.split(',')]
            try:
                return {'key': key, 'values': [int(v) for v in values]}
            except ValueError:
                return {'key': key, 'values': values}
        else:
            try:
                return {'key': key, 'value': int(value)}
            except ValueError:
                return {'key': key, 'value': value}
    
    def _handle_power_state(self, state: int):
        """Handle power state change from MCU"""
        if state == 0:  # Power off
            if not self.poweroff_scheduled:
                self.poweroff_scheduled = True
                command = self.config['power']['poweroff_command']
                self.logger.info("Power off requested. Preparing for shutdown...")
                
                def execute_poweroff():
                    try:
                        # Turn off LED first
                        self._turn_off_led()
                        
                        # Sync filesystems for safe shutdown
                        subprocess.run(['sync'], check=True, timeout=5)
                        self.logger.info("Filesystems synced")

                        # Send shutdown confirmation to MCU
                        if not self._send_message("shutdown_ready=1"):
                            self.logger.warning("Failed to send shutdown confirmation to MCU")

                        # Wait a moment for message to be sent
                        time.sleep(0.1)

                        # Log success message
                        self.logger.info("Shutdown complete, Mynd MCU notified - powering off")

                        # Then power off
                        subprocess.run(command.split(), check=True, timeout=10)
                    except subprocess.TimeoutExpired:
                        self.logger.error("Poweroff command timed out")
                    except Exception as e:
                        self.logger.error(f"Failed to execute poweroff: {e}", exc_info=True)
                
                # Execute poweroff in a separate thread to avoid blocking
                threading.Thread(target=execute_poweroff, daemon=False).start()
            else:
                self.logger.debug("Power off already scheduled, ignoring duplicate request")
        elif state == 1:  # Power on
            self.poweroff_scheduled = False
            self.logger.info("Power on state received")
    
    def _handle_button_event(self, button_bitfield: int, input_state: int):
        """Handle button event from MCU"""
        self.logger.info(f"Button event: bitfield={button_bitfield}, state={input_state}")
        
        # Forward to Moode API based on button and state
        if button_bitfield & BUTTON_PLUS:
            if input_state == INPUT_STATE_SHORT_PRESS or input_state == INPUT_STATE_HOLD:
                self.logger.info("Volume up button pressed")
                self._moode_volume_up()
        elif button_bitfield & BUTTON_MINUS:
            if input_state == INPUT_STATE_SHORT_PRESS or input_state == INPUT_STATE_HOLD:
                self.logger.info("Volume down button pressed")
                self._moode_volume_down()
        elif button_bitfield & BUTTON_PLAY:
            if input_state == INPUT_STATE_SHORT_PRESS:
                self.logger.info("Play button pressed")
                self._moode_toggle_playback()
            elif input_state == INPUT_STATE_DOUBLE_PRESS or input_state == INPUT_STATE_DOUBLE_RELEASE:
                self.logger.info("Play button double pressed - next radio station")
                self._moode_next_station()
            elif input_state == INPUT_STATE_TRIPLE_PRESS or input_state == INPUT_STATE_TRIPLE_RELEASE:
                self.logger.info("Play button triple pressed - previous radio station")
                self._moode_previous_station()
        elif button_bitfield & BUTTON_BT:
            if input_state == INPUT_STATE_SHORT_PRESS:
                self.logger.info("BT button pressed")
                self._moode_handle_bt_button()
        elif button_bitfield & BUTTON_POWER:
            if input_state == INPUT_STATE_SHORT_PRESS:
                self.logger.info("Power button pressed")
                self._moode_handle_power_button()
    
    def _moode_volume_up(self):
        """Increase volume via Moode REST API"""
        # Use Moode REST API set_volume with -up to update web interface
        cmd = quote('set_volume -up 5')
        self._moode_api_call('GET', f'/command/?cmd={cmd}')
    
    def _moode_volume_down(self):
        """Decrease volume via Moode REST API"""
        # Use Moode REST API set_volume with -dn to update web interface
        cmd = quote('set_volume -dn 5')
        self._moode_api_call('GET', f'/command/?cmd={cmd}')
    
    def _moode_toggle_playback(self):
        """Toggle play/pause via Moode REST API"""
        # Use Moode REST API toggle_play_pause to update web interface
        cmd = quote('toggle_play_pause')
        self._moode_api_call('GET', f'/command/?cmd={cmd}')
    
    def _moode_next_station(self):
        """Go to next radio station via Moode API"""
        # Moode's MPD command format: next (plays next item in queue)
        cmd = quote('next')
        self._moode_api_call('GET', f'/command/?cmd={cmd}')
    
    def _moode_previous_station(self):
        """Go to previous radio station via Moode API"""
        # Moode's MPD command format: previous (plays previous item in queue)
        cmd = quote('previous')
        self._moode_api_call('GET', f'/command/?cmd={cmd}')
    
    def _moode_handle_bt_button(self):
        """Handle BT button press (template for extension)"""
        self.logger.debug("BT button pressed - template for custom action")
        # Add custom BT button handling here
    
    def _moode_handle_power_button(self):
        """Handle Power button press (template for extension)"""
        self.logger.debug("Power button pressed - template for custom action")
        # Add custom power button handling here
    
    def _moode_get_volume(self) -> Optional[int]:
        """Get current volume from Moode REST API"""
        base_url = self.config['moode']['base_url']
        timeout = float(self.config['moode']['api_timeout'])
        retry_count = int(self.config['moode']['retry_count'])
        
        cmd = quote('get_volume')
        url = f"{base_url}/command/?cmd={cmd}"
        
        for attempt in range(retry_count):
            try:
                response = requests.get(url, timeout=timeout, allow_redirects=True, verify=False)
                
                if response.status_code == 200:
                    try:
                        data = response.json()
                        volume_str = data.get('volume', '0')
                        volume = int(volume_str)
                        return volume
                    except (json.JSONDecodeError, ValueError, KeyError) as e:
                        self.logger.warning(f"Failed to parse volume response: {e}, response: {response.text[:200]}")
                        return None
                else:
                    if attempt < retry_count - 1:
                        time.sleep(0.5)
                    else:
                        self.logger.warning(f"Moode API get_volume: HTTP {response.status_code}")
            except requests.exceptions.RequestException as e:
                if attempt < retry_count - 1:
                    time.sleep(0.5)
                else:
                    self.logger.warning(f"Moode API get_volume failed: {e}")
        
        return None
    
    def _moode_set_volume(self, volume: int) -> bool:
        """Set volume via Moode REST API"""
        cmd = quote(f'set_volume {volume}')
        return self._moode_api_call('GET', f'/command/?cmd={cmd}')
    
    def _check_moode_response_error(self, response_text: str, method: str, endpoint: str) -> bool:
        """Check if Moode API response contains an error. Returns True if error found."""
        try:
            text = response_text.strip()
            if text and ('error' in text.lower() or 'ack' in text.lower()):
                self.logger.warning(f"Moode API {method} {endpoint}: Command error in response: {text[:200]}")
                return True
        except:
            pass
        return False
    
    def _moode_api_call(self, method: str, endpoint: str, data: Optional[Dict] = None) -> bool:
        """Make HTTP request to Moode API"""
        base_url = self.config['moode']['base_url']
        timeout = float(self.config['moode']['api_timeout'])
        retry_count = int(self.config['moode']['retry_count'])
        verify = False  # Don't verify SSL certificates for localhost
        
        url = f"{base_url}{endpoint}"
        
        for attempt in range(retry_count):
            try:
                # Make request
                if method == 'POST':
                    response = requests.post(url, json=data, timeout=timeout, allow_redirects=True, verify=verify)
                elif method == 'GET':
                    response = requests.get(url, timeout=timeout, allow_redirects=True, verify=verify)
                else:
                    self.logger.error(f"Unsupported HTTP method: {method}")
                    return False
                
                # Handle redirects
                if response.status_code in (301, 302):
                    final_url = response.url
                    self.logger.warning(f"Moode API {method} {endpoint}: Redirected to {final_url}, status {response.status_code}")
                    if method == 'GET':
                        response = requests.get(final_url, timeout=timeout, verify=verify)
                    else:
                        response = requests.post(final_url, json=data, timeout=timeout, verify=verify)
                
                # Check response
                if response.status_code == 200:
                    if self._check_moode_response_error(response.text, method, endpoint):
                        return False
                    self.logger.info(f"Moode API {method} {endpoint}: OK")
                    return True
                else:
                    self.logger.warning(f"Moode API {method} {endpoint}: HTTP {response.status_code}")
                    try:
                        self.logger.warning(f"Moode API error response: {response.text[:200]}")
                    except:
                        pass
                    
            except (requests.exceptions.ConnectionError, requests.exceptions.Timeout, requests.exceptions.RequestException) as e:
                is_last_attempt = (attempt == retry_count - 1)
                if is_last_attempt:
                    error_type = type(e).__name__.replace('Exception', '').replace('Request', '')
                    self.logger.error(f"Moode API {error_type} failed after {retry_count} attempts: {e}")
                    self.logger.error(f"  URL: {url}")
                    if isinstance(e, requests.exceptions.ConnectionError):
                        self.logger.error(f"  Check if Moode is running and accessible at {base_url}")
                    return False
                else:
                    self.logger.debug(f"Moode API request failed (attempt {attempt + 1}/{retry_count}): {e}")
                    time.sleep(0.5)
        
        return False
    
    def _process_message(self, msg: Dict[str, Any]):
        """Process parsed message from MCU"""
        key = msg['key']
        
        if key == 'state':
            self._handle_power_state(msg['value'])
        elif key == 'button':
            if 'values' in msg and len(msg['values']) == 2:
                self._handle_button_event(msg['values'][0], msg['values'][1])
        elif key in ['battery', 'charging', 'aux']:
            # Log system status updates
            self.logger.debug(f"System status: {key}={msg['value']}")
        else:
            self.logger.debug(f"Received message: {key}={msg.get('value', msg.get('values', 'unknown'))}")
    
    def _rx_loop(self):
        """Receive and process messages from MCU"""
        buf = bytearray()
        
        while self.running:
            if not self.ser or not self.ser.is_open:
                time.sleep(1)
                continue
            
            try:
                b = self.ser.read(1)
                if not b:
                    continue
                
                if b == b'\n':
                    line = buf.decode('utf-8', errors='replace')
                    buf.clear()
                    
                    if line:
                        msg = self._parse_message(line)
                        if msg:
                            self._process_message(msg)
                else:
                    buf.extend(b)
                    # Prevent buffer overflow
                    if len(buf) > 256:
                        self.logger.warning("RX buffer overflow, clearing")
                        buf.clear()
                        
            except serial.SerialException as e:
                self.logger.error(f"Serial error: {e}")
                time.sleep(1)
                # Try to reopen
                self._close_uart()
                time.sleep(1)
                self._open_uart()
            except Exception as e:
                self.logger.error(f"Error in RX loop: {e}")
                time.sleep(0.1)
    
    def run(self):
        """Main daemon loop"""
        self.logger.info("Starting Mynd RPi Link Daemon")
        
        if not self._open_uart():
            self.logger.error("Failed to open UART, exiting")
            return 1
        
        self.running = True
        
        # Start RX thread
        rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        rx_thread.start()
        
        # Send startup commands after a brief delay to ensure UART is ready
        time.sleep(0.2)
        self._initialize_mcu()
        
        # Main loop - keep daemon alive
        try:
            while self.running:
                time.sleep(1)
        except KeyboardInterrupt:
            self.logger.info("Received interrupt signal")
        finally:
            self.running = False
            # Turn off LED before shutdown
            self._turn_off_led()
            self._close_uart()
            self.logger.info("Mynd RPi Link Daemon stopped successfully")
        
        return 0
    
    def stop(self):
        """Stop the daemon"""
        self.running = False


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Mynd RPi Link Daemon')
    parser.add_argument('--config', default='/etc/mynd_rpi_daemon.conf',
                       help='Path to configuration file')
    parser.add_argument('--foreground', action='store_true',
                       help='Run in foreground (for debugging)')
    
    args = parser.parse_args()
    
    # Check if config file exists
    if not os.path.exists(args.config):
        print(f"ERROR: Configuration file not found: {args.config}", file=sys.stderr)
        print(f"Please create a configuration file or use --config to specify a different path", file=sys.stderr)
        sys.exit(1)
    
    daemon = MyndRpiDaemon(args.config)
    
    # Setup signal handlers
    def signal_handler(sig, frame):
        daemon.stop()
    
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    return daemon.run()


if __name__ == '__main__':
    sys.exit(main())

