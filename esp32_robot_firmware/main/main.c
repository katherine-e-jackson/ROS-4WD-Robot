#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "motor_control.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "booting");

    esp_err_t err = motor_control_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "motor_control_init failed: %s", esp_err_to_name(err));
        return;
    }

    motor_enable(true);

    wheel_id_t wheels[] = {
        WHEEL_FRONT_LEFT,
        WHEEL_FRONT_RIGHT,
        WHEEL_BACK_LEFT,
        WHEEL_BACK_RIGHT,
        WHEEL_COUNT
    };

    const char *wheel_names[] = {
        "FRONT_LEFT",
        "FRONT_RIGHT",
        "BACK_LEFT",
        "BACK_RIGHT"
    };

   
    motor_stop_all();
}