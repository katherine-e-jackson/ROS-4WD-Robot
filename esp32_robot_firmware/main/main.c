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

    // for (int i = 0; i < 4; i++) {
    //     ESP_LOGI(TAG, "Testing wheel: %s FORWARD (50%%)", wheel_names[i]);
    //     motor_set_speed(wheels[i], 50);
    //     vTaskDelay(pdMS_TO_TICKS(2000));

    //     ESP_LOGI(TAG, "Testing wheel: %s REVERSE (50%%)", wheel_names[i]);
    //     motor_set_speed(wheels[i], -50);
    //     vTaskDelay(pdMS_TO_TICKS(2000));

    //     ESP_LOGI(TAG, "Stopping wheel: %s", wheel_names[i]);
    //     motor_set_speed(wheels[i], 0);
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    // motor_enable(true);
    motor_set_speed(WHEEL_FRONT_LEFT, 100);    // hold 30 s, probe the driver's BIN1/BIN2
  
    vTaskDelay(pdMS_TO_TICKS(30000));
    motor_set_speed(WHEEL_FRONT_LEFT, -100);   // hold 30 s, probe again
    vTaskDelay(pdMS_TO_TICKS(30000));

    ESP_LOGI(TAG, "Test complete. Stopping all wheels.");
    motor_stop_all();
}