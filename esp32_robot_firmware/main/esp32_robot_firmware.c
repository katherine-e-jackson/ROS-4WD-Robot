#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "motor_control.h"
#include "ble_control.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "booting");

    // Bluetooth/Wi-Fi RF calibration data is stored in NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = motor_control_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "motor_control_init failed: %s", esp_err_to_name(err));
        return;
    }

    motor_enable(true);

    err = ble_control_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ble_control_init failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "ready — waiting for BLE commands");

    // app_main can return now; ble_control runs its own FreeRTOS task
    // and motor commands come in through its write callback from here on.
}
