#pragma once

/**
 * @file furi_hal_target_hw.h
 * @brief Which optional HAL modules exist on this board.
 *
 * Stock f7 pulls in NFC, RFID and iButton headers here. This board has none of
 * them; it does have Sub-GHz and IR.
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */

#include <furi_hal_subghz.h>
#include <furi_hal_infrared.h>
