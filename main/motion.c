// IMU sampling task.
#include <math.h>
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensors.h"
#undef M_PI  // qmi8658.h defines its own
#include "qmi8658.h"

static const char *TAG = "motion";

static qmi8658_dev_t imu;
static motion_analysis_t analysis;
static motion_features_t latest;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

static void motion_task(void *arg)
{
    TickType_t wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / IMU_RATE_HZ);
    int errors = 0;
    for (;;) {
        vTaskDelayUntil(&wake, period);
        qmi8658_data_t d;
        if (qmi8658_read_sensor_data(&imu, &d) != ESP_OK) {
            if (++errors % 200 == 1) ESP_LOGW(TAG, "IMU read failed (%d)", errors);
            continue;
        }
        // Driver reports mg and deg/s.
        const float acc[3] = { d.accelX / 1000.0f, d.accelY / 1000.0f, d.accelZ / 1000.0f };
        const float gyro[3] = { d.gyroX, d.gyroY, d.gyroZ };
        motion_analysis_update(&analysis, acc, gyro);
        taskENTER_CRITICAL(&lock);
        latest = analysis.out;
        taskEXIT_CRITICAL(&lock);
    }
}

esp_err_t motion_start(void)
{
    motion_analysis_init(&analysis, IMU_RATE_HZ);
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    ESP_RETURN_ON_FALSE(bus, ESP_FAIL, TAG, "no I2C bus");
    ESP_RETURN_ON_ERROR(qmi8658_init(&imu, bus, QMI8658_ADDRESS_HIGH), TAG, "IMU init failed");
    qmi8658_set_accel_range(&imu, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(&imu, QMI8658_ACCEL_ODR_250HZ);
    qmi8658_set_gyro_range(&imu, QMI8658_GYRO_RANGE_2048DPS);  // fast twists exceed 500 deg/s
    qmi8658_set_gyro_odr(&imu, QMI8658_GYRO_ODR_250HZ);
    BaseType_t ok = xTaskCreatePinnedToCore(motion_task, "motion", 4096, NULL, 5, NULL, 0);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void motion_get(motion_features_t *out)
{
    taskENTER_CRITICAL(&lock);
    *out = latest;
    taskEXIT_CRITICAL(&lock);
}
