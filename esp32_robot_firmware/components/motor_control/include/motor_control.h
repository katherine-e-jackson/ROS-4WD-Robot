#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Left Driver (Driver A)  -> Back-Left (2), Front-Left (0)
   Right Driver (Driver B) -> Back-Right (3), Front-Right (1) */
typedef enum {
    WHEEL_BACK_LEFT   = 2,
    WHEEL_FRONT_LEFT  = 0,
    WHEEL_BACK_RIGHT  = 3,
    WHEEL_FRONT_RIGHT = 1,
    WHEEL_COUNT       = 4
} wheel_id_t;

/**
 * @brief Initialize GPIO + LEDC PWM for all four wheels and the shared
 *        STBY line. Motors are disabled (STBY low) after init.
 */
esp_err_t motor_control_init(void);

/**
 * @brief Set a single wheel's speed.
 * @param wheel  Which wheel to drive.
 * @param speed  -100 (full reverse) to 100 (full forward). 0 = coast.
 */
esp_err_t motor_set_speed(wheel_id_t wheel, int8_t speed);

/**
 * @brief Set all four wheels at once (e.g. from differential-drive kinematics).
 */
esp_err_t motor_set_all(int8_t front_left, int8_t front_right,
                         int8_t rear_left, int8_t rear_right);

/**
 * @brief Enable or disable both drivers via the shared STBY pin.
 *        Used as the hardware-level e-stop.
 */
void motor_enable(bool enable);

/**
 * @brief Immediately zero all wheel speeds. Does NOT touch STBY.
 */
 void motor_stop_all(void);


#ifdef __cplusplus
}
#endif