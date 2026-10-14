#include "motor_control.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "motor_control";

/* Driver A (left), A -> back-left (2)  | B -> front-left (0)
   Driver B (right), A -> front-right (1) | B -> back-right (3) */

// Shared standby
#define PIN_STBY        GPIO_NUM_13

// Driver A (Left Side)
#define PIN_A_PWMA      GPIO_NUM_14   // Back-Left speed      
#define PIN_A_AIN1      GPIO_NUM_26   // Back-Left direction
#define PIN_A_AIN2      GPIO_NUM_27   // Back-Left direction
#define PIN_A_PWMB      GPIO_NUM_32   // Front-Left speed     
#define PIN_A_BIN1      GPIO_NUM_25   // Front-Left direction
#define PIN_A_BIN2      GPIO_NUM_33   // Front-Left direction

// Driver B (Right Side)
#define PIN_B_PWMA      GPIO_NUM_23    // Front-Right speed    
#define PIN_B_AIN1      GPIO_NUM_18    // Front-Right direction
#define PIN_B_AIN2      GPIO_NUM_19   // Front-Right direction
#define PIN_B_PWMB      GPIO_NUM_4    // Back-Right speed    
#define PIN_B_BIN1      GPIO_NUM_17   // Back-Right direction
#define PIN_B_BIN2      GPIO_NUM_16   // Back-Right direction

/* PWM configuration */
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_FREQ_HZ        5000
#define LEDC_DUTY_RES       LEDC_TIMER_10_BIT   // duty range 0–1023
#define LEDC_MAX_DUTY       ((1 << 10) - 1)

// Cap applied for TT motors
#define MOTOR_MAX_DUTY_PERCENT  70

typedef struct {
    gpio_num_t pwm_pin;
    gpio_num_t dir1_pin;
    gpio_num_t dir2_pin;
    ledc_channel_t ledc_channel;
} wheel_pins_t;

static const wheel_pins_t s_wheel_pins[WHEEL_COUNT] = {
    [WHEEL_FRONT_LEFT]  = { PIN_A_PWMB, PIN_A_BIN1, PIN_A_BIN2, LEDC_CHANNEL_0 },
    [WHEEL_FRONT_RIGHT] = { PIN_B_PWMA, PIN_B_AIN1, PIN_B_AIN2, LEDC_CHANNEL_1 },
    [WHEEL_BACK_LEFT]   = { PIN_A_PWMA, PIN_A_AIN1, PIN_A_AIN2, LEDC_CHANNEL_2 },
    [WHEEL_BACK_RIGHT]  = { PIN_B_PWMB, PIN_B_BIN1, PIN_B_BIN2, LEDC_CHANNEL_3 },
};

static esp_err_t init_direction_pins(void)
{
    uint64_t pin_mask = 0;
    for (int i = 0; i < WHEEL_COUNT; i++) {
        pin_mask |= (1ULL << s_wheel_pins[i].dir1_pin);
        pin_mask |= (1ULL << s_wheel_pins[i].dir2_pin);
    }
    pin_mask |= (1ULL << PIN_STBY);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io_conf);
}

static esp_err_t init_pwm(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) return err;

    for (int i = 0; i < WHEEL_COUNT; i++) {
        ledc_channel_config_t ch_conf = {
            .gpio_num = s_wheel_pins[i].pwm_pin,
            .speed_mode = LEDC_MODE,
            .channel = s_wheel_pins[i].ledc_channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER,
            .duty = 0,
            .hpoint = 0,
        };
        err = ledc_channel_config(&ch_conf);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

esp_err_t motor_control_init(void)
{
    esp_err_t err = init_direction_pins();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "direction pin init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = init_pwm();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PWM init failed: %s", esp_err_to_name(err));
        return err;
    }

    motor_enable(false);   // stay disabled until something explicitly enables us
    motor_stop_all();

    ESP_LOGI(TAG, "motor_control initialized");
    return ESP_OK;
}

esp_err_t motor_set_speed(wheel_id_t wheel, int8_t speed)
{
    if (wheel >= WHEEL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;

    const wheel_pins_t *w = &s_wheel_pins[wheel];

    // Direction
    if (speed > 0) {
        gpio_set_level(w->dir1_pin, 1);
        gpio_set_level(w->dir2_pin, 0);
    } else if (speed < 0) {
        gpio_set_level(w->dir1_pin, 0);
        gpio_set_level(w->dir2_pin, 1);
    } else {
        // Coast: both direction pins low
        gpio_set_level(w->dir1_pin, 0);
        gpio_set_level(w->dir2_pin, 0);
    }

    // Speed -> duty, with the protective cap applied
    int magnitude = abs(speed);
    int capped = (magnitude * MOTOR_MAX_DUTY_PERCENT) / 100;
    uint32_t duty = (capped * LEDC_MAX_DUTY) / 100;

    esp_err_t err = ledc_set_duty(LEDC_MODE, w->ledc_channel, duty);
    if (err != ESP_OK) return err;
    return ledc_update_duty(LEDC_MODE, w->ledc_channel);
}

esp_err_t motor_set_all(int8_t front_left, int8_t front_right,
                         int8_t rear_left, int8_t rear_right)
{
    esp_err_t err;
    if ((err = motor_set_speed(WHEEL_FRONT_LEFT, front_left)) != ESP_OK) return err;
    if ((err = motor_set_speed(WHEEL_FRONT_RIGHT, front_right)) != ESP_OK) return err;
    if ((err = motor_set_speed(WHEEL_BACK_LEFT, rear_left)) != ESP_OK) return err;
    if ((err = motor_set_speed(WHEEL_BACK_RIGHT, rear_right)) != ESP_OK) return err;
    return ESP_OK;
}

void motor_enable(bool enable)
{
    gpio_set_level(PIN_STBY, enable ? 1 : 0);
    ESP_LOGI(TAG, "motors %s", enable ? "enabled" : "disabled (STBY low)");
}

void motor_stop_all(void)
{
    for (int i = 0; i < WHEEL_COUNT; i++) {
        motor_set_speed((wheel_id_t)i, 0);
    }
}