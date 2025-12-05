#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "tas5805m.h"
#include "tas5825p.h"

typedef enum
{
    AMP_MODE_NORMAL,
    AMP_MODE_BYPASS,
} board_link_amps_mode_t;

typedef enum
{
    AMP_ENVELOPE_TRACKING_MODE_OFF_MIN_PVDD,
    AMP_ENVELOPE_TRACKING_MODE_OFF_MAX_PVDD,
    AMP_ENVELOPE_TRACKING_MODE_ON,
} board_link_amps_envelope_tracking_mode_t;

#if defined(__cplusplus)
extern "C"
{
#endif

    void board_link_amps_init(void);

    void board_link_amps_enable(bool enable);

    int board_link_amps_set_hi_z(void);

    int board_link_amps_setup_woofer(board_link_amps_mode_t mode);

    int board_link_amps_setup_tweeter(board_link_amps_mode_t mode);

    void board_link_amps_enable_eco_mode(bool enable);

    void board_link_amps_set_envelope_tracking_mode(board_link_amps_envelope_tracking_mode_t mode);

    void board_link_amps_enable_eq(bool enable);

    /**
     * @brief Sets the digital volume control for both amps.
     *
     * @details The volume range goes from -90 dB to +10 dB.
     *          Anything less than -90 dB gets written as -infinite dB.
     *
     * @param[in] h             pointer to handler
     * @param[in] volume_db     volume in dB
     */
    void board_link_amps_set_volume(int8_t volume_db);

    /**
     * @brief Sets the bass level on the woofer amp.
     *
     * @details The supported bass levels are -6 dB to +6 dB in 1 dB steps
     *
     * @param[in] h             pointer to handler
     * @param[in] bass_db       bass level in dB
     */
    void board_link_amps_set_bass_level(int8_t bass_db);

    /**
     * @brief Sets the treble level on the woofer amp.
     *
     * @details The supported treble levels are -6 dB to +6 dB in 1 dB steps
     *
     * @param[in] h             pointer to handler
     * @param[in] treble_db     treble level in dB
     */
    void board_link_amps_set_treble_level(int8_t treble_db);

    /**
     * @brief Sets the digital volume control for the tweeter amp.
     *
     * @details The volume range goes from -90 dB to +10 dB.
     *          Anything less than -90 dB gets written as -infinite dB.
     *
     * @param[in] h             pointer to handler
     * @param[in] volume_db     volume in dB
     */
    void board_link_amps_set_tweeter_volume(int8_t volume_db);

    /**
     * @brief Sets the digital volume control for the woofer amp.
     *
     * @details The volume range goes from -90 dB to +10 dB.
     *          Anything less than -90 dB gets written as -infinite dB.
     *
     * @param[in] h             pointer to handler
     * @param[in] volume_db     volume in dB
     */
    void board_link_amps_set_woofer_volume(int8_t volume_db);

    int8_t board_link_amps_get_tweeter_volume(void);
    int8_t board_link_amps_get_woofer_volume(void);

    void board_link_amps_mute(bool enable);

    bool board_link_amps_is_muted(void);

    void board_link_amps_toggle_mute(void);

    bool board_link_amps_woofer_fault_detected(void);
    void board_link_amps_woofer_fault_recover(void);

    int board_link_amps_read_fs_mon(tas5825p_fs_t *p_woofer_fs, tas5805m_fs_t *p_tweeter_fs);

    bool board_link_amps_fs_ready(void);

#if defined(__cplusplus)
}
#endif
