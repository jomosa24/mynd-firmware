#include "actionslink.h"

int actionslink_init(const actionslink_config_t *p_config, const actionslink_event_handlers_t *p_event_handlers,
                     const actionslink_request_handlers_t *p_request_handlers)
{
    return 0;
}

int actionslink_deinit(void)
{
    return 0;
}

void actionslink_force_stop(void)
{
}

void actionslink_tick(void)
{
}

bool actionslink_is_ready(void)
{
    return true;
}

bool actionslink_is_busy(void)
{
    return false;
}

int actionslink_get_firmware_version(actionslink_firmware_version_t *p_version)
{
    return 0;
}

int actionslink_set_power_state(actionslink_power_state_t power_state)
{
    return 0;
}

int actionslink_enter_dfu_mode(void)
{
    return 0;
}

int actionslink_get_this_device_name(actionslink_buffer_dsc_t *p_buffer_dsc)
{
    return 0;
}

int actionslink_get_bt_device_name(uint64_t address, actionslink_buffer_dsc_t *p_buffer_dsc)
{
    return 0;
}

int actionslink_get_bt_mac_address(uint64_t *p_bt_mac_address)
{
    return 0;
}

int actionslink_get_ble_mac_address(uint64_t *p_ble_mac_address)
{
    return 0;
}

int actionslink_get_bt_rssi_value(int8_t *p_bt_rssi_val)
{
    return 0;
}

int actionslink_get_bt_paired_device_list(actionslink_bt_paired_device_list_t *p_list)
{
    return 0;
}

int actionslink_clear_bt_paired_device_list(void)
{
    return 0;
}

int actionslink_disconnect_all_bt_devices(void)
{
    return 0;
}

int actionslink_increase_volume(void)
{
    return 0;
}

int actionslink_decrease_volume(void)
{
    return 0;
}

int actionslink_set_bt_absolute_avrcp_volume(uint8_t avrcp_volume)
{
    return 0;
}

int actionslink_bt_play_pause(void)
{
    return 0;
}

int actionslink_bt_play(void)
{
    return 0;
}

int actionslink_bt_pause(void)
{
    return 0;
}

int actionslink_bt_next_track(void)
{
    return 0;
}

int actionslink_bt_previous_track(void)
{
    return 0;
}

int actionslink_start_bt_pairing(void)
{
    return 0;
}

int actionslink_start_multichain_pairing(void)
{
    return 0;
}

int actionslink_start_csb_broadcaster(void)
{
    return 0;
}

int actionslink_start_csb_receiver(void)
{
    return 0;
}

int actionslink_stop_pairing(void)
{
    return 0;
}

int actionslink_exit_csb_mode(actionslink_csb_master_exit_reason_t reason)
{
    return 0;
}

int actionslink_enable_bt_reconnection(bool enable)
{
    return 0;
}

int actionslink_send_aux_connection_notification(bool is_connected)
{
    return 0;
}

int actionslink_send_usb_connection_notification(bool is_connected)
{
    return 0;
}

int actionslink_send_battery_level(uint8_t battery_level)
{
    return 0;
}

int actionslink_send_charger_status(actionslink_charger_status_t status)
{
    return 0;
}

int actionslink_send_battery_friendly_charging_notification(bool status)
{
    return 0;
}

int actionslink_send_eco_mode_state(bool state)
{
    return 0;
}

int actionslink_send_color_id(actionslink_device_color_t color)
{
    return 0;
}

int actionslink_usb_play_pause(void)
{
    return 0;
}

int actionslink_usb_next_track(void)
{
    return 0;
}

int actionslink_usb_previous_track(void)
{
    return 0;
}

int actionslink_set_audio_source(actionslink_audio_source_t source)
{
    return 0;
}

int actionslink_play_sound_icon(actionslink_sound_icon_t sound_icon,
                                actionslink_sound_icon_playback_mode_t playback_mode, bool loop_forever)
{
    return 0;
}

int actionslink_stop_sound_icon(actionslink_sound_icon_t sound_icon)
{
    return 0;
}

int actionslink_send_set_off_timer_response(uint8_t sequence_id, actionslink_error_t result)
{
    return 0;
}

int actionslink_send_set_brightness_response(uint8_t sequence_id, actionslink_error_t result)
{
    return 0;
}

int actionslink_send_set_bass_response(uint8_t sequence_id, actionslink_error_t result)
{
    return 0;
}

int actionslink_send_set_treble_response(uint8_t sequence_id, actionslink_error_t result)
{
    return 0;
}

int actionslink_send_set_eco_mode_response(uint8_t sequence_id, actionslink_error_t result)
{
    return 0;
}

int actionslink_send_set_sound_icons_response(uint8_t sequence_id, actionslink_error_t result)
{
    return 0;
}

int actionslink_send_set_battery_friendly_charging_response(uint8_t sequence_id, actionslink_error_t result)
{
    return 0;
}

int actionslink_send_get_mcu_firmware_version_response(uint8_t sequence_id,
    uint32_t major, uint32_t minor, uint32_t patch, const char *build)
{
    return 0;
}

int actionslink_send_get_pdcontroller_firmware_version_response(uint8_t sequence_id, uint32_t major, uint32_t minor)
{
    return 0;
}

int actionslink_send_get_serial_number_response(uint8_t sequence_id, const char *serial_number)
{
    return 0;
}

int actionslink_send_get_color_response(uint8_t sequence_id, actionslink_device_color_t color)
{
    return 0;
}

int actionslink_send_get_off_timer_response(uint8_t sequence_id, bool is_enabled, uint32_t value)
{
    return 0;
}

int actionslink_send_get_brightness_response(uint8_t sequence_id, uint32_t value)
{
    return 0;
}

int actionslink_send_get_bass_response(uint8_t sequence_id, int8_t value)
{
    return 0;
}

int actionslink_send_get_treble_response(uint8_t sequence_id, int8_t value)
{
    return 0;
}

int actionslink_send_get_eco_mode_response(uint8_t sequence_id, bool is_enabled)
{
    return 0;
}

int actionslink_send_get_sound_icons_response(uint8_t sequence_id, bool is_enabled)
{
    return 0;
}

int actionslink_send_get_battery_friendly_charging_response(uint8_t sequence_id, bool is_enabled)
{
    return 0;
}

int actionslink_send_get_battery_capacity_response(uint8_t sequence_id, uint32_t value)
{
    return 0;
}

int actionslink_send_get_battery_max_capacity_response(uint8_t sequence_id, uint32_t value)
{
    return 0;
}

const char *actionslink_get_version()
{
    return "stub";
}

int actionslink_write_key_value(uint32_t key, uint32_t value)
{
    return 0;
}

int actionslink_read_key_value(uint32_t key, uint32_t *p_value)
{
    return 0;
}

