#include "ble_control.h"
#include "motor_control.h"

#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble_control";

#define DRIVE_SPEED 50  

// Custom 128-bit UUIDs (randomly generated, not a standard service).
// Visible in LightBlue / nRF Connect as a service with one writable characteristic.
static const ble_uuid128_t SERVICE_UUID =
    BLE_UUID128_INIT(0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,
                      0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88);
static const ble_uuid128_t CMD_CHAR_UUID =
    BLE_UUID128_INIT(0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,
                      0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x89);

static uint8_t own_addr_type;

static void apply_command(char cmd)
{
    switch (cmd) {
        case 'w':
            ESP_LOGI(TAG, "cmd: forward");
            motor_set_all(DRIVE_SPEED, DRIVE_SPEED, DRIVE_SPEED, DRIVE_SPEED);
            break;
        case 's':
            ESP_LOGI(TAG, "cmd: reverse");
            motor_set_all(-DRIVE_SPEED, -DRIVE_SPEED, -DRIVE_SPEED, -DRIVE_SPEED);
            break;
        case 'a':
            ESP_LOGI(TAG, "cmd: pivot left");
            motor_set_all(-DRIVE_SPEED, DRIVE_SPEED, -DRIVE_SPEED, DRIVE_SPEED);
            break;
        case 'd':
            ESP_LOGI(TAG, "cmd: pivot right");
            motor_set_all(DRIVE_SPEED, -DRIVE_SPEED, DRIVE_SPEED, -DRIVE_SPEED);
            break;
        case ' ':
            ESP_LOGI(TAG, "cmd: stop");
            motor_stop_all();
            break;
        default:
            ESP_LOGW(TAG, "unknown command byte: 0x%02x", (unsigned char)cmd);
            break;
    }
}

static int cmd_chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint8_t buf[8];
    uint16_t len = 0;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &len);
    if (rc != 0 || len == 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    buf[len] = '\0';

    apply_command((char)buf[0]);
    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SERVICE_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &CMD_CHAR_UUID.u,
                .access_cb = cmd_chr_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            { 0 }
        },
    },
    { 0 } 
};

static int gap_event_cb(struct ble_gap_event *event, void *arg);

static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)"ESP32-Robot";
    fields.name_len = strlen("ESP32-Robot");
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                       &adv_params, gap_event_cb, NULL);
    ESP_LOGI(TAG, "advertising started as 'ESP32-Robot'");
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "central connected (status=%d)", event->connect.status);
            if (event->connect.status != 0) {
                start_advertising(); // connection failed, resume advertising
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "central disconnected, stopping motors");
            motor_stop_all();
            start_advertising();
            break;
        default:
            break;
    }
    return 0;
}

static void on_sync(void)
{
    ble_hs_id_infer_auto(0, &own_addr_type);
    start_advertising();
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_control_init(void)
{
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ble_hs_cfg.sync_cb = on_sync;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    ble_svc_gap_device_name_set("ESP32-Robot");

    nimble_port_freertos_init(host_task);

    ESP_LOGI(TAG, "ble_control initialized");
    return ESP_OK;
}