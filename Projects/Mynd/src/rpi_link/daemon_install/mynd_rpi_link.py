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
import socket
import subprocess
import sys
import time
import threading
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

# PowerState enum values (from MCU)
POWER_STATE_OFF = 0
POWER_STATE_ON = 1
POWER_STATE_PRE_OFF = 2
POWER_STATE_PRE_ON = 3
POWER_STATE_TRANSITION = 4


class MyndRpiDaemon:
    """Main daemon class for Mynd RPi Link"""
    
    def __init__(self, config_path: str):
        self.config = self._load_config(config_path)
        self.running = False
        self.ser: Optional[serial.Serial] = None
        self.logger = self._setup_logging()
        self.poweroff_scheduled = False
        self.current_power_state = POWER_STATE_OFF
        self.preparing_shutdown = False  # Track if we've prepared for shutdown (PreOff received)
        self.mcu_initialized = False  # Track if MCU has been initialized for current power state
        self.shutdown_prep_thread: Optional[threading.Thread] = None  # Reference to shutdown prep thread
        self.charging_active = False  # Track if charging is active
        self.aux_jack_connected = False  # Track if aux jack is connected
        self.battery_level = 0  # Track battery level
        self._streaming_monitor_thread: Optional[threading.Thread] = None
        self._streaming_monitor_stop = threading.Event()
        self._last_streaming_active: Optional[bool] = None
        self._streaming_poll_interval = float(
            self.config['moode'].get('streaming_poll_interval', '5.0')
        )
        self._mpd_port = int(self.config['moode'].get('mpd_port', '6600'))
        self._request_id_counter = 0  # Request ID counter for commands
        self._bfc_state: Optional[bool] = None  # Track BFC state

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
                'retry_count': '3',
                'streaming_poll_interval': '5.0',
                'mpd_port': '6600'
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
        logger = logging.getLogger('mynd_rpi_link')
        
        use_syslog = self.config['logging'].get('use_syslog', 'true').lower() == 'true'
        level_str = self.config['logging'].get('level', 'INFO').upper()
        level = getattr(logging, level_str, logging.INFO)
        
        logger.setLevel(level)
        
        stdout_formatter = logging.Formatter('%(asctime)s [%(levelname)s] %(message)s')
        
        if use_syslog:
            # Use syslog handler
            try:
                handler = logging.handlers.SysLogHandler(address='/dev/log')
                handler.setFormatter(logging.Formatter('mynd_rpi_link[%(process)d]: %(message)s'))
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
        # Stop streaming monitor before closing UART
        self._stop_streaming_monitor()
        
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
                self.logger.info("Closed UART connection")
            except Exception as e:
                self.logger.error(f"Error closing UART: {e}")
    
    def _turn_off_led(self):
        """Turn off LED 2"""
        try:
            if self._send_to_mcu("set led_2 0 0 0"):
                self.logger.info("Turned off LED 2")
            else:
                self.logger.warning("Failed to turn off LED 2")
            time.sleep(0.1)
        except Exception as e:
            self.logger.error(f"Error turning off LED: {e}")
    
    def _send_to_mcu(self, data: str, request_id: Optional[int] = None) -> bool:
        """Send command or message to MCU with new protocol format"""
        if not self.ser or not self.ser.is_open:
            return False
        
        try:
            # If no request ID provided, use counter and increment
            if request_id is None:
                request_id = self._request_id_counter
                self._request_id_counter = (self._request_id_counter + 1) % 10000  # Wrap at 10000
            
            formatted = f"rpi:{request_id}:{data}"
            if not formatted.endswith('\n'):
                formatted += '\n'
            self.ser.write(formatted.encode())
            self.ser.flush()
            self.logger.debug(f"Sent to MCU: {formatted.strip()}")
            return True
        except Exception as e:
            self.logger.error(f"Failed to send to MCU: {e}")
            return False
    
    def _send_status_update(self, status: int) -> bool:
        """Send RPI status update to MCU
        
        Status values:
        - 0 = RPiReady
        - 1 = RPiNotReady
        - 2 = RPiError
        - 3 = RPiUnknown
        """
        status_names = {0: 'RPiReady', 1: 'RPiNotReady', 2: 'RPiError', 3: 'RPiUnknown'}
        status_name = status_names.get(status, f'UNKNOWN({status})')
        self.logger.info(f"Sending RPI status update to MCU: {status_name} ({status})")
        result = self._send_to_mcu(f"set status {status}")
        if not result:
            self.logger.error(f"Failed to send RPI status update: {status_name} ({status})")
        return result
    
    def _initialize_mcu(self):
        """Initialize MCU with startup commands"""
        try:
            self.logger.info("Initializing MCU: enabling all periodic updates and setting LED 2 to Raspberry Pi color")
            
            # Enable periodic system updates (battery, charging, state, aux)
            if self._send_to_mcu("set send_system_snapshots 1"):
                self.logger.info("Enabled periodic system updates")
            else:
                self.logger.warning("Failed to enable periodic system updates")
            time.sleep(0.1)
            
            # Enable button event forwarding
            if self._send_to_mcu("set send_button_events 1"):
                self.logger.info("Enabled button event forwarding")
            else:
                self.logger.warning("Failed to enable button event forwarding")
            time.sleep(0.1)
            
            # Set LED 2 to Raspberry Pi color (official Raspberry Pi red: RGB 227, 27, 27)
            # Note: LED 2 doesn't require override_power (only LED 1 does)
            if self._send_to_mcu("set led_2 227 27 27"):
                self.logger.info("Set LED 2 to Raspberry Pi color (227, 27, 27)")
            else:
                self.logger.warning("Failed to set LED 2 color")
            time.sleep(0.1)
            
            # Check Moode volume and set to 5% if it's 0% for audible ready chime
            current_volume = self._moode_get_volume()
            if current_volume is not None:
                if current_volume == 0:
                    self.logger.info("Moode volume is 0%, setting to 5%")
                    if not self._moode_set_volume(5):
                        self.logger.warning("Failed to set Moode volume to 5%")
            else:
                self.logger.warning("Failed to get Moode volume, skipping volume check")
            
            self.mcu_initialized = True
            self.logger.info("MCU initialization complete")
            
            # Send RPiReady status to MCU (send before starting streaming monitor to ensure it's sent)
            if not self._send_status_update(0):  # RPiReady
                self.logger.warning("Failed to send RPiReady status update")
            time.sleep(0.1)  # Give MCU time to process the status update
            
            # Start streaming monitor after MCU is initialized
            self._start_streaming_monitor()
        except Exception as e:
            self.logger.error(f"Error during MCU initialization: {e}", exc_info=True)
            # Send RPiError status to MCU
            self._send_status_update(2)  # RPiError
    
    def _parse_message(self, line: str) -> Optional[Dict[str, Any]]:
        """Parse message from MCU"""
        line = line.strip()
        if not line:
            return None
        
        if line.startswith('mcu:notify:'):
            rest = line[11:]
            if '=' not in rest:
                return None
            key, value = rest.split('=', 1)
            msg = {'type': 'notify', 'key': key}
            # Handle comma-separated values
            if ',' in value:
                values = [v.strip() for v in value.split(',')]
                try:
                    msg['values'] = [int(v) for v in values]
                except ValueError:
                    msg['values'] = values
            else:
                try:
                    msg['value'] = int(value)
                except ValueError:
                    msg['value'] = value
            return msg
        elif line.startswith('mcu:'):
            rest = line[4:]
            if ':' not in rest:
                return None
            id_str, rest = rest.split(':', 1)
            try:
                msg_id = int(id_str)
            except ValueError:
                return None
            
            if rest.startswith('error '):
                # Error response
                return {'type': 'error', 'id': msg_id, 'message': rest[6:]}
            elif '=' in rest:
                # Key-value response
                key, value = rest.split('=', 1)
                msg = {'type': 'response', 'id': msg_id, 'key': key}
                # Handle comma-separated values
                if ',' in value:
                    values = [v.strip() for v in value.split(',')]
                    try:
                        msg['values'] = [int(v) for v in values]
                    except ValueError:
                        msg['values'] = values
                else:
                    try:
                        msg['value'] = int(value)
                    except ValueError:
                        msg['value'] = value
                return msg
        
        return None
    
    def _handle_power_state(self, state: int):
        """Handle power state change from MCU
        
        Handles three explicit power states:
        - POWER_STATE_PRE_OFF (2): Prepare for shutdown (sync, turn off LED, send shutdown_ready)
        - POWER_STATE_OFF (0): Actually power off (only sent if charging is not active)
        - POWER_STATE_PRE_ON (3) or POWER_STATE_ON (1): Power on
        """
        previous_state = self.current_power_state
        self.current_power_state = state
        
        if state == POWER_STATE_PRE_OFF:
            self.logger.info("PreOff state received - preparing for shutdown")
            # Pause streaming if active
            self._moode_pause_playback()
            self.mcu_initialized = False  # Reset initialization flag when going to PreOff
            self._prepare_shutdown()
        elif state == POWER_STATE_OFF:
            self.mcu_initialized = False
            if not self.poweroff_scheduled:
                self.poweroff_scheduled = True
                threading.Thread(target=self._execute_poweroff, daemon=False).start()
            else:
                self.logger.debug("Power off already scheduled, ignoring duplicate request")
        elif state == POWER_STATE_PRE_ON or state == POWER_STATE_ON:
            self.poweroff_scheduled = False
            self.preparing_shutdown = False
            state_name = "PreOn" if state == POWER_STATE_PRE_ON else "On"
            if self.mcu_initialized == False:
                self._initialize_mcu()
            
        else:
            self.logger.warning(f"Unknown power state received: {state}")
    
    def _prepare_shutdown(self):
        """Prepare for shutdown: sync filesystems, turn off LED, and notify MCU"""
        if self.preparing_shutdown:
            return
        else:
            self.preparing_shutdown = True
        
        def _do_shutdown_prep():
            try:
                subprocess.run(['sync'], check=True, timeout=5)
                self.logger.info("Filesystems synced")
                self._turn_off_led()
                time.sleep(0.1)
                self.logger.info("Shutdown preparation complete")
            except Exception as e:
                self.logger.error(f"Failed to prepare shutdown: {e}", exc_info=True)
        
        self.shutdown_prep_thread = threading.Thread(target=_do_shutdown_prep, daemon=False)
        self.shutdown_prep_thread.start()
    
    def _execute_poweroff(self):
        """Execute the actual poweroff command"""
        try:
            # Disable periodic updates immediately (synchronously) to stop MCU from sending messages
            self._send_to_mcu("set send_system_snapshots 0")
            time.sleep(0.1)
            self._send_to_mcu("set send_button_events 0")
            time.sleep(0.1)
            
            # Wait for shutdown prep thread to complete
            if self.shutdown_prep_thread is not None:
                self.logger.info("Waiting for shutdown preparation to complete...")
                self.shutdown_prep_thread.join()
                self.logger.info("Shutdown preparation completed, proceeding with poweroff")
                if not self._send_to_mcu("shutdown_ready"):
                    self.logger.warning("Failed to send shutdown confirmation to MCU")
            
            self.logger.info("Power off command received - executing shutdown")
            command = self.config['power']['poweroff_command']
            subprocess.run(command.split(), check=True, timeout=10)
        except subprocess.TimeoutExpired:
            self.logger.error("Poweroff command timed out")
        except Exception as e:
            self.logger.error(f"Failed to execute poweroff: {e}", exc_info=True)
    
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
            elif input_state == INPUT_STATE_DOUBLE_RELEASE:
                self.logger.info("Play button double pressed - next radio station")
                self._moode_next_station()
            elif input_state == INPUT_STATE_TRIPLE_RELEASE:
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
    
    def _moode_pause_playback(self):
        """Pause playback via Moode REST API (only pauses if currently playing)"""
        playback_state = self._moode_get_playback_state()
        if playback_state == 'play':
            # Use Moode REST API pause command
            cmd = quote('pause')
            self._moode_api_call('GET', f'/command/?cmd={cmd}')
            self.logger.info("Playback paused")
        else:
            self.logger.debug("Playback not active, skipping pause")
    
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
    
    def _query_mpd_status(self, sock: socket.socket) -> Optional[str]:
        """Query MPD status over an already-connected socket."""
        try:
            # Read the initial greeting (OK MPD version)
            greeting = sock.recv(1024).decode('utf-8')
            if not greeting.startswith('OK MPD'):
                self.logger.debug("Unexpected MPD greeting: %s", greeting[:50])
                return None
            
            # Send status command
            sock.sendall(b'status\n')
            
            # Read response until we get OK
            response = b''
            while True:
                chunk = sock.recv(1024)
                if not chunk:
                    break
                response += chunk
                if b'\nOK\n' in response or response.endswith(b'\nOK\n'):
                    break
            
            # Parse response for state line
            for line in response.decode('utf-8').split('\n'):
                line = line.strip()
                if line.startswith('state: '):
                    state = line.split(':', 1)[1].strip()
                    if state:
                        return state.lower()
            
            self.logger.debug("MPD status response did not contain state line")
            return None
        except Exception as exc:
            self.logger.debug("Error reading MPD response: %s", exc)
            return None
    
    def _try_mpd_connection(self, address, is_unix_socket=False) -> Optional[str]:
        """Try to connect to MPD and query status. Returns state string or None."""
        try:
            if is_unix_socket:
                sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                sock.settimeout(2.0)
                sock.connect(address)
            else:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(2.0)
                host, port = address
                sock.connect((host, port))
            
            result = self._query_mpd_status(sock)
            sock.close()
            return result
        except (FileNotFoundError, socket.timeout, socket.error, Exception):
            return None
    
    def _moode_get_playback_state(self) -> Optional[str]:
        """Fetch playback state from MPD using the MPD protocol."""
        # Try Unix socket first (standard location on Moode)
        result = self._try_mpd_connection('/run/mpd/socket', is_unix_socket=True)
        if result is not None:
            return result
        
        # Fall back to TCP
        result = self._try_mpd_connection(('localhost', self._mpd_port), is_unix_socket=False)
        if result is not None:
            return result
        
        return None
    
    def _determine_streaming_active(self) -> Optional[bool]:
        """Interpret MPD playback state into a streaming-active boolean.
        
        According to MPD protocol documentation (https://mpd.readthedocs.io/en/latest/protocol.html),
        the state field only returns:
        - 'play': player is playing (active)
        - 'pause': player is paused (inactive)
        - 'stop': player is stopped (inactive)
        """
        state = self._moode_get_playback_state()
        if state is None:
            return None
        
        # MPD protocol only defines three state values
        if state == 'play':
            return True
        elif state in ('pause', 'stop'):
            return False
        
        # Unknown state value - log warning and return None
        self.logger.warning("Unknown MPD state value: %s", state)
        return None
    
    def _send_streaming_active_state(self, is_active: bool) -> bool:
        """Notify the MCU about the current streaming state."""
        value = 1 if is_active else 0
        return self._send_to_mcu(f"set streaming_active {value}")
    
    def _streaming_state_loop(self):
        """Background loop that polls MPD and notifies the MCU of streaming state changes."""
        self.logger.info("Streaming monitor thread started")
        poll_interval = max(1.0, self._streaming_poll_interval)
        consecutive_failures = 0
        
        while not self._streaming_monitor_stop.is_set():
            if not self.mcu_initialized:
                self._streaming_monitor_stop.wait(poll_interval)
                continue
            
            is_streaming = self._determine_streaming_active()
            
            if is_streaming is not None and is_streaming != self._last_streaming_active:
                self._last_streaming_active = is_streaming
                consecutive_failures = 0
                self.logger.info("Streaming active changed to %s; notifying MCU",
                                 "true" if is_streaming else "false")
                self._send_streaming_active_state(is_streaming)
            elif is_streaming is None:
                consecutive_failures += 1
                # Only log warning every 10 failures to avoid spam
                if consecutive_failures == 10:
                    self.logger.warning("Streaming state unavailable from MPD; check MPD is running")
            
            self._streaming_monitor_stop.wait(poll_interval)
        
        self.logger.info("Streaming monitor thread stopped")
    
    def _start_streaming_monitor(self):
        """Launch the streaming monitor thread."""
        if self._streaming_monitor_thread and self._streaming_monitor_thread.is_alive():
            return
        
        self._streaming_monitor_stop.clear()
        self._streaming_monitor_thread = threading.Thread(
            target=self._streaming_state_loop,
            name="StreamingMonitor",
            daemon=True,
        )
        self._streaming_monitor_thread.start()
    
    def _stop_streaming_monitor(self):
        """Signal the streaming monitor thread to stop."""
        self._streaming_monitor_stop.set()
        thread = self._streaming_monitor_thread
        if thread and thread.is_alive():
            thread.join(timeout=1.0)
        self._streaming_monitor_thread = None
        self._last_streaming_active = None
    
    def _process_message(self, msg: Dict[str, Any]):
        """Process parsed message from MCU"""
        msg_type = msg.get('type', 'legacy')
        
        if msg_type == 'notify':
            # Unsolicited notification from MCU
            key = msg['key']
            if key == 'state':
                state_value = msg['value']
                state_names = {0: 'OFF', 1: 'ON', 2: 'PRE_OFF', 3: 'PRE_ON', 4: 'TRANSITION'}
                state_name = state_names.get(state_value, f'UNKNOWN({state_value})')
                self.logger.debug(f"Received power state notification: {state_name} ({state_value})")
                self._handle_power_state(state_value)
            elif key == 'button':
                if 'values' in msg and len(msg['values']) == 2:
                    self._handle_button_event(msg['values'][0], msg['values'][1])
            elif key in ['battery', 'charging', 'aux', 'bfc']:
                # Log system status updates
                self.logger.debug(f"System status notification: {key}={msg['value']}")
                if key == 'battery':
                    self.battery_level = msg['value']
                elif key == 'charging':
                    # If charging moves from active to inactive and mcu is not initialized (mynd in standby), execute poweroff
                    # Unfortunately, this is not reliable since the MCU may not send the notification quickly enough after the USB is unplugged
                    if bool(msg['value']) == False and self.charging_active and not self.mcu_initialized:
                        self._execute_poweroff()
                    self.charging_active = bool(msg['value'])
                elif key == 'aux':
                    self.aux_jack_connected = bool(msg['value'])
                elif key == 'bfc':
                    self._bfc_state = bool(msg['value'])
            else:
                self.logger.debug(f"Received notification: {key}={msg.get('value', msg.get('values', 'unknown'))}")
        elif msg_type == 'response':
            # Response to a command
            key = msg['key']
            if key == 'bfc':
                self._bfc_state = bool(msg['value'])
            elif key == 'error':
                self.logger.warning(f"MCU returned error: {msg.get('message', 'unknown error')}")
            else:
                self.logger.debug(f"Received response: {key}={msg.get('value', msg.get('values', 'unknown'))}")
    
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
                # Send RPiError status on serial errors
                try:
                    if self.ser and self.ser.is_open:
                        self._send_status_update(2)  # RPiError
                except:
                    pass
                time.sleep(1)
                # Try to reopen
                self._close_uart()
                time.sleep(1)
                if self._open_uart():
                    # If UART reopens successfully, send RPiReady status
                    self._send_status_update(0)  # RPiReady
            except Exception as e:
                self.logger.error(f"Error in RX loop: {e}")
                time.sleep(0.1)
    
    def run(self):
        """Main daemon loop"""
        self.logger.info("Starting Mynd RPi Link Daemon")
        
        if not self._open_uart():
            self.logger.error("Failed to open UART, exiting")
            # Send RPiError status before exiting
            try:
                if self.ser and self.ser.is_open:
                    self._send_status_update(2)  # RPiError
            except:
                pass
            return 1
        
        self.running = True
        
        # Start RX thread
        rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        rx_thread.start()
        
        # Send startup commands after a brief delay to ensure UART is ready
        time.sleep(0.2)
        self._initialize_mcu()
        self.mcu_initialized = True  # Mark as initialized after startup
        
        # Main loop - keep daemon alive
        try:
            while self.running:
                time.sleep(1)
        except KeyboardInterrupt:
            self.logger.info("Received interrupt signal")
        finally:
            self.running = False
            # Send RPiNotReady status before shutdown
            self._send_status_update(1)  # RPiNotReady
            # Stop streaming monitor
            self._stop_streaming_monitor()
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
    parser.add_argument('--config', default='/etc/mynd_rpi_link.conf',
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
