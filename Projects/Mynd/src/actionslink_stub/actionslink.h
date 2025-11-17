#pragma once

#include "actionslink_types.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    /**
     * @brief Initializes the Actionslink library.
     * @note  This function prepares all internal structures, buffers, context, etc.
     *        It does not start any communication with the Actions module.
     *        The first interaction occurs in the first `actionslink_tick()` call.
     *
     * @param[in] p_config              pointer to configuration
     * @param[in] p_event_handlers      pointer to event handlers
     * @param[in] p_request_handlers    pointer to request handlers
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_init(const actionslink_config_t *p_config, const actionslink_event_handlers_t *p_event_handlers,
                         const actionslink_request_handlers_t *p_request_handlers);

    /**
     * @brief Deinitializes the Actionslink library.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_deinit(void);

    /**
     * @brief Forces the Actionslink library to stop processing commands.
     * @note  The library needs to be reinitialized after a force stop.
     */
    void actionslink_force_stop(void);

    /**
     * @brief Reads from the buffers and generates events, if any.
     */
    void actionslink_tick(void);

    /**
     * @brief Checks if the Actionslink library is ready to use (handshake done, etc).
     *
     * @return true if ready, false otherwise
     */
    bool actionslink_is_ready(void);

    /**
     * @brief Checks if the Actionslink library is currently busy processing a command.
     * @note  This function is useful to check if commands can be sent from
     *        Actionslink event handlers.
     *
     * @return true if busy, false otherwise
     */
    bool actionslink_is_busy(void);

    /**
     * @brief Gets the firmware version of the Actions module.
     *
     * @param[out] p_version    pointer to the version struct where the version data will be written to
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_get_firmware_version(actionslink_firmware_version_t *p_version);

    /**
     * @brief Sets the power state of the Actions module.
     *
     * @param[in] power_state  power state to set
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_set_power_state(actionslink_power_state_t power_state);

    /**
     * @brief Requests the Actions module to enter DFU mode.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_enter_dfu_mode(void);

    /**
     * @brief Gets the name of this device.
     * @note  If the device name does not fit in the buffer, this function will still return successfully,
     *        but the name will be truncated. The string will be null-terminated.
     *
     * @param[inout] p_buffer_dsc   pointer to the descriptor of the buffer where the name will be written to
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_get_this_device_name(actionslink_buffer_dsc_t *p_buffer_dsc);

    /**
     * @brief Gets the name of a Bluetooth device.
     * @note  If the device name does not fit in the buffer, this function will still return successfully,
     *        but the name will be truncated. The string will be null-terminated.
     *
     * @param[in]    p_address      address of device to request the name of
     * @param[inout] p_buffer_dsc   pointer to the descriptor of the buffer where the name will be written to
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_get_bt_device_name(uint64_t address, actionslink_buffer_dsc_t *p_buffer_dsc);

    /**
     * @brief Gets the mac address of the Bluetooth module.
     *
     * @param[inout] p_bt_mac_address   pointer to the where the bluetooth mac address will be written to in big endian decimal format
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_get_bt_mac_address(uint64_t *p_bt_mac_address);

    /**
     * @brief Gets the mac address of the Bluetooth Low Energy module.
     *
     * @param[inout] p_ble_mac_address   pointer to the where the bluetooth low energy mac address will be written to in big endian decimal format
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_get_ble_mac_address(uint64_t *p_ble_mac_address);

    /**
     * @brief Gets the RSSI value of the Bluetooth module.
     *
     * @param[inout] p_bt_rssi_val   pointer to the where the bluetooth rssi value will be written to
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_get_bt_rssi_value(int8_t *p_bt_rssi_val);

    /**
     * @brief Gets the list of paired devices.
     * @note  The address list struct needs to be initialized before passing it to this function.
     *
     *        uint64_t addresses[SIZE_OF_LIST];
     *        actionslink_bt_paired_device_list_t paired_device_list = {
     *            .p_list = addresses,
     *            .list_size = SIZE_OF_LIST,
     *            .number_of_items = 0,
     *        };
     *
     *        The number of items in the list will be correctly written if the function returns
     *        successfully.
     *
     * @param[inout] p_list             pointer to the list struct to fill
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_get_bt_paired_device_list(actionslink_bt_paired_device_list_t *p_list);

    /**
     * @brief Disconnects all Bluetooth devices and deletes the list of known paired devices.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_clear_bt_paired_device_list(void);

    /**
     * @brief Disconnects all Bluetooth devices.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_disconnect_all_bt_devices(void);

    /**
     * @brief Sends a command to the Actions module to increase the volume by one step.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_increase_volume(void);

    /**
     * @brief Sends a command to the Actions module to decrease the volume by one step.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_decrease_volume(void);

    /**
     * @brief Sends a command to the Actions module to tell the Bluetooth device
     *        to set the AD2P volume using an absolute AVRCP volume value.
     *
     * @details This command only works if an A2DP source is selected.
     *          The AVRCP volume range is 0-127.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_set_bt_absolute_avrcp_volume(uint8_t avrcp_volume);

    /**
     * @brief Sends a command to the Actions module to tell the Bluetooth device to play/pause audio.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_bt_play_pause(void);

    /**
     * @brief Sends a command to the Actions module to tell the Bluetooth device to play audio.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_bt_play(void);

    /**
     * @brief Sends a command to the Actions module to tell the Bluetooth device to pause audio.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_bt_pause(void);

    /**
     * @brief Sends a command to the Actions module to tell the Bluetooth device to go to the next track.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_bt_next_track(void);

    /**
     * @brief Sends a command to the Actions module to tell the Bluetooth device to go to the previous track.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_bt_previous_track(void);

    /**
     * @brief Sends a command to the Actions module to start Bluetooth pairing.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_start_bt_pairing(void);

    /**
     * @brief Sends a command to the Actions module to start CSB pairing.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_start_multichain_pairing(void);

#ifdef INCLUDE_TWS_MODE
    /**
     * @brief Sends a command to the Actions module to start TWS pairing.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_start_tws_pairing(void);

    /**
     * @brief Sends a command to the Actions module to start TWS pairing as master.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_start_tws_pairing_as_master(void);

    /**
     * @brief Sends a command to the Actions module to start TWS pairing as slave.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_start_tws_pairing_as_slave(void);

    /**
     * @brief Sends a command to the Actions module to exit TWS mode.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_exit_tws_mode(void);
#endif // INCLUDE_TWS_MODE

    /**
     * @brief Sends a command to the Actions module to exit CSB mode.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_exit_csb_mode(actionslink_csb_master_exit_reason_t reason);

    /**
     * @brief Sends a command to the Actions module to start CSB broadcasting.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_start_csb_broadcaster(void);

    /**
     * @brief Sends a command to the Actions module to start CSB receiving.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_start_csb_receiver(void);

    /**
     * @brief Sends a command to the Actions module to stop any ongoing pairing modes.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_stop_pairing(void);

    /**
     * @brief Sends a command to the Actions module to enable/disable reconnection to known devices.
     *
     * @param[in] enable        enable option
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_enable_bt_reconnection(bool enable);

    /**
     * @brief Sends a command to the Actions module to tell the USB device to play/pause audio.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_usb_play_pause(void);

    /**
     * @brief Sends a command to the Actions module to tell the USB device to stop audio.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_usb_stop(void);

    /**
     * @brief Sends a command to the Actions module to tell the USB device to skip forwards (next track).
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_usb_next_track(void);

    /**
     * @brief Sends a command to the Actions module to tell the USB device to skip backwards (previous track).
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_usb_previous_track(void);

    /**
     * @brief Sends a command to the Actions module to tell the USB device to mute/unmute itself.
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_usb_mute(void);

    /**
     * @brief Sends a command to the Actions module to set an audio source
     *
     * @param[in] source        audio source to set
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_set_audio_source(actionslink_audio_source_t source);

    /**
     * @brief Sends a notification to the Actions module regarding the state of the aux connection.
     *
     * @param[in] is_connected      true if aux is connected, false otherwise
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_aux_connection_notification(bool is_connected);

    /**
     * @brief Sends a notification to the Actions module regarding the state of the USB connection.
     *
     * @param[in] is_connected      true if USB is connected, false otherwise
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_usb_connection_notification(bool is_connected);

    /**
     * @brief Sends the current battery level of the speaker to the Actions module.
     *
     * @param[in] battery_level     battery level
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_battery_level(uint8_t battery_level);

    /**
     * @brief Sends the current charger status of the speaker to the Actions module.
     *
     * @param[in] status          charger status
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_charger_status(actionslink_charger_status_t status);

    /**
     * @brief Sends the current current Eco mode state
     *
     * @param[in] state           eco mode state
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_eco_mode_state(bool status);

    /**
     * @brief Sends the color id.
     *
     * @param[in] color         color id
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_color_id(actionslink_device_color_t color);

    /**
     * @brief Sends the battery friendly charging status.
     *
     * @param status            battery friendly charging status
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_battery_friendly_charging_notification(bool status);

    /**
     * @brief Sends a command to the Actions module to play a sound icon.
     *
     * @param[in] sound_icon        sound icon to play
     * @param[in] playback_mode     playback mode
     * @param[in] loop_forever      true if the sound icon should be looped forever, false otherwise
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_play_sound_icon(actionslink_sound_icon_t               sound_icon,
                                    actionslink_sound_icon_playback_mode_t playback_mode, bool loop_forever);

    /**
     * @brief Sends a command to the Actions module to stop playing a sound icon.
     * @note If the sound icon does not match the currently playing sound icon, the BT module will do nothing.
     *
     * @param[in] sound_icon        sound icon to stop
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_stop_sound_icon(actionslink_sound_icon_t sound_icon);

    /**
     * @brief Gets the version of the Actionslink library.
     *
     * @return 0 if successful, -1 otherwise
     */
    const char *actionslink_get_version();

    /**
     * @brief Requests a read from the Actions module's persistent storage 
     *
     * @param[in] key            ID of stored value
     * @param[in] p_value        pointer to where to store value
     *
     * @return 0 if successful, -1 if key does not exist or is unavailable, 
     *         otherwise -2 for any other error
     */
    int actionslink_read_key_value(uint32_t key, uint32_t *p_value);

    /**
     * @brief Requests a write to the Actions module's persistent storage 
     *
     * @param[in] key            ID of value to be stored
     * @param[in] value          value to store
     *
     * @return 0 if successful, otherwise -2 for any other error
     */
    int actionslink_write_key_value(uint32_t key, uint32_t value);

    /**
     * @brief Sends the response to the set off timer command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] a_error        actionslink error code
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_set_off_timer_response(uint8_t sequence_id, actionslink_error_t a_error);
    int actionslink_send_set_brightness_response(uint8_t sequence_id, actionslink_error_t a_error);
    int actionslink_send_set_bass_response(uint8_t sequence_id, actionslink_error_t a_error);
    int actionslink_send_set_treble_response(uint8_t sequence_id, actionslink_error_t a_error);
    int actionslink_send_set_eco_mode_response(uint8_t sequence_id, actionslink_error_t a_error);
    int actionslink_send_set_sound_icons_response(uint8_t sequence_id, actionslink_error_t a_error);
    int actionslink_send_set_battery_friendly_charging_response(uint8_t sequence_id, actionslink_error_t result);

    /**
     * @brief Sends the response to the get mcu firmware version command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] major          major version number
     * @param[in] minor          minor version number
     * @param[in] patch          patch version number
     * @param[in] build          build version number
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_mcu_firmware_version_response(uint8_t sequence_id,
        uint32_t major, uint32_t minor, uint32_t patch, const char *build);

    /**
     * @brief Sends the response to the get PD controller firmware version command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] major          major version number
     * @param[in] minor          minor version number
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_pdcontroller_firmware_version_response(uint8_t sequence_id, uint32_t major, uint32_t minor);

    /**
     * @brief Sends the response to the get serial number command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] serial_number  serial number of the speaker
     *
     * @return 0 if successful, -1 otherwise
     */
#ifdef ActionsLink_FromMcuResponse_get_serial_number_tag
    int actionslink_send_get_serial_number_response(uint8_t sequence_id, const char *serial_number);
#endif

    /**
     * @brief Sends the response to the get color command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] color          color of the speaker
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_color_response(uint8_t sequence_id, actionslink_device_color_t color);

    /**
     * @brief Sends the response to the get off timer command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] is_enabled     true if the off timer is enabled, false otherwise
     * @param[in] value          off timer value
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_off_timer_response(uint8_t sequence_id, bool is_enabled, uint32_t value);

    /**
     * @brief Sends the response to the get brightness command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] value          brightness value
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_brightness_response(uint8_t sequence_id, uint32_t value);

    /**
     * @brief Sends the response to the get bass command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] value          bass value
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_bass_response(uint8_t sequence_id, int8_t value);

    /**
     * @brief Sends the response to the get treble command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] value          treble value
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_treble_response(uint8_t sequence_id, int8_t value);

    /**
     * @brief Sends the response to the get eco mode command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] is_enabled     true if eco mode is enabled, false otherwise
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_eco_mode_response(uint8_t sequence_id, bool is_enabled);

    /**
     * @brief Sends the response to the get sound icons command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] is_enabled     true if sound icons are enabled, false otherwise
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_sound_icons_response(uint8_t sequence_id, bool is_enabled);

    /**
     * @brief Sends the response to the get battery friendly charging command to the Actions module.
     *
     * @param[in] sequence_id    sequence ID of the command
     * @param[in] is_enabled     true if friendly charging enabled, false otherwise
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_battery_friendly_charging_response(uint8_t sequence_id, bool is_enabled);

    /**
     * @brief Sends the response to the get battery capacity command to the Actions module.
     *
     * @param[in] sequence_id   sequence ID of the command
     * @param[in] value         battery capacity (mAh)
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_battery_capacity_response(uint8_t sequence_id, uint32_t value);

    /**
     * @brief Sends the response to the get battery MAX capacity command to the Actions module.
     *
     * @param[in] sequence_id   sequence ID of the command
     * @param[in] value         battery MAX capacity (mAh)
     *
     * @return 0 if successful, -1 otherwise
     */
    int actionslink_send_get_battery_max_capacity_response(uint8_t sequence_id, uint32_t value);

#if defined(__cplusplus)
}
#endif

