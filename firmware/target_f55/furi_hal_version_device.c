/**
 * @file furi_hal_version_device.c
 * @brief Board identity strings and target check for the "f55" target.
 *
 * The numeric board version still comes from OTP (see README §5). We accept
 * target 7 (a stock-compatible OTP blob), 55 (a dedicated blob) and 0 (blank
 * OTP, bench bring-up) so the firmware is not bricked by an unprogrammed part.
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */

#include <furi_hal_version.h>

bool furi_hal_version_do_i_belong_here(void) {
    uint8_t target = furi_hal_version_get_hw_target();
    return (target == 7) || (target == 55) || (target == 0);
}

const char* furi_hal_version_get_model_name(void) {
    return "WB55 Custom Clone";
}

const char* furi_hal_version_get_model_code(void) {
    return "WB55.1";
}

const char* furi_hal_version_get_fcc_id(void) {
    return "Pending";
}

const char* furi_hal_version_get_ic_id(void) {
    return "Pending";
}

const char* furi_hal_version_get_mic_id(void) {
    return "Pending";
}

const char* furi_hal_version_get_srrc_id(void) {
    return "Pending";
}

const char* furi_hal_version_get_ncc_id(void) {
    return "Pending";
}
