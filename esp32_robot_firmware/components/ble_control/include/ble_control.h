#pragma once
 
#include "esp_err.h"
 
#ifdef __cplusplus
extern "C" {
#endif
 
/**
 * @brief Start BLE advertising and the GATT server. Once laptop 
 *        connects and writes to the command characteristic,
 *        single-character commands drive the robot via motor_control:
 *          'w' = forward      's' = reverse
 *          'a' = pivot left    'd' = pivot right
 *          ' ' (space) = stop
 */
esp_err_t ble_control_init(void);
 
#ifdef __cplusplus
}
#endif
 