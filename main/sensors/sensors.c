#include "sensors.h"

#include <esp_log.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "filters/median.h"
#include "filters/kalman.h"

#define SENSORS_REFRESH_RATE 250
#define SENSORS_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_VREF
#define SENSORS_VBAT_CH  ADC1_CHANNEL_7
#define SENSORS_VBAT_ATT ADC_ATTEN_DB_11
#define SENSORS_VBAT_MUL (0.00151f)
#define SENSORS_VBAT_MAX (4.05f)
#define SENSORS_VBAT_MIN (2.9f)
#define SENSORS_CHRG_CH  ADC1_CHANNEL_6
#define SENSORS_CHRG_ATT ADC_ATTEN_DB_2_5
#define SENSORS_CHRG_MUL (0.001f)
#define SENSORS_CHRG_MIN (0.085f)

static bool adc_calibrated;
static esp_adc_cal_characteristics_t vbat_ch_chars;
static esp_adc_cal_characteristics_t chrg_ch_chars;

static int vbat_raw_value;
static int chrg_raw_value;
static float vbat_value;
static float chrg_value;
static median_t *vbat_filter_1;
static kalman_t *vbat_filter_2;
static median_t *chrg_filter_1;
static kalman_t *chrg_filter_2;

static float bat_level;
static int bat_is_charging;

static sens_callback_t callbacks[SENS_COUNT] = {0};

static void adc_calibration_init(void);
static float recalculate_vbat(float voltage);
static float recalculate_chrg(float voltage);
static float recalculate_level(float voltage);

void sensors_register_callback(sens_t sensor, sens_callback_t clbk)
{
    callbacks[sensor] = clbk;
}

float sensors_get_value(sens_t sensor)
{
    switch (sensor)
    {
    case SENS_VBAT:
        return vbat_value;
    case SENS_CHRG:
        return chrg_value;
    case SENS_BAT_LEVEL:
        return bat_level;
    case SENS_BAT_CHARGING:
        return bat_is_charging;
    default:
        return 0.0f;
    }
}

void sensors_task(void *arg)
{
    esp_err_t err;
    float new_vbat_value = 0.0f;
    float new_chrg_value = 0.0f;
    float new_bat_raw_level = 100.0f;
    float new_bat_level = 100.0f;
    int new_bat_is_charging = 0;

    err = adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
    if (err != ESP_OK)
    {
        ESP_LOGE("sens", "ADC width configuration error");
        goto error;
    }

    err = adc1_config_channel_atten(SENSORS_VBAT_CH, SENSORS_VBAT_ATT);
    if (err != ESP_OK)
    {
        ESP_LOGE("sens", "ADC battery voltage channel configuration error");
        goto error;
    }

    err = adc1_config_channel_atten(SENSORS_CHRG_CH, SENSORS_CHRG_ATT);
    if (err != ESP_OK)
    {
        ESP_LOGE("sens", "ADC charge current channel configuration error");
        goto error;
    }

    adc_calibration_init();

    vbat_filter_1 = median_init();
    vbat_filter_2 = kalman_init(10.0f, 0.1f);
    chrg_filter_1 = median_init();
    chrg_filter_2 = kalman_init(10.0f, 0.1f);
    if (vbat_filter_1 == NULL || vbat_filter_2 == NULL ||
        chrg_filter_1 == NULL || chrg_filter_2 == NULL)
    {
        ESP_LOGE("sens", "filters initialization error");
        goto error;
    }

    vTaskDelay(1);
    for (int i = 0; i < 6; ++i)
    {
        vbat_raw_value = adc1_get_raw(SENSORS_VBAT_CH);
        chrg_raw_value = adc1_get_raw(SENSORS_CHRG_CH);
        new_vbat_value = recalculate_vbat(esp_adc_cal_raw_to_voltage(vbat_raw_value, &vbat_ch_chars));
        new_chrg_value = recalculate_chrg(esp_adc_cal_raw_to_voltage(chrg_raw_value, &chrg_ch_chars));
    }
    vbat_value = new_vbat_value;
    chrg_value = new_chrg_value;
    new_bat_is_charging = new_chrg_value > SENSORS_CHRG_MIN;
    new_bat_raw_level = recalculate_level(new_vbat_value);
    new_bat_level = new_bat_raw_level;
    bat_level = new_bat_level;
    bat_is_charging = new_bat_is_charging;

    while(1)
    {
        vbat_raw_value = adc1_get_raw(SENSORS_VBAT_CH);
        chrg_raw_value = adc1_get_raw(SENSORS_CHRG_CH);
        
        if (adc_calibrated)
        {
            new_vbat_value = recalculate_vbat(esp_adc_cal_raw_to_voltage(vbat_raw_value, &vbat_ch_chars));
            new_chrg_value = recalculate_chrg(esp_adc_cal_raw_to_voltage(chrg_raw_value, &chrg_ch_chars));

            new_bat_is_charging = new_chrg_value > SENSORS_CHRG_MIN;
            new_bat_raw_level = recalculate_level(new_vbat_value);

            if (new_bat_is_charging)
            {
                new_bat_level = new_bat_raw_level;
            }
            else if (new_bat_raw_level < new_bat_level)
            {
                new_bat_level = new_bat_raw_level;
            }

            if (new_bat_level > 100)
                new_bat_level = 100;
            else if (new_bat_level < 0)
                new_bat_level = 0;
        }

        // execute callbacks
        if (new_vbat_value != vbat_value && callbacks[SENS_VBAT] != NULL)
            callbacks[SENS_VBAT](SENS_VBAT, new_vbat_value);
        if (new_chrg_value != chrg_value && callbacks[SENS_CHRG] != NULL)
            callbacks[SENS_CHRG](SENS_CHRG, new_chrg_value);
        if (new_bat_level != bat_level && callbacks[SENS_BAT_LEVEL] != NULL)
            callbacks[SENS_BAT_LEVEL](SENS_BAT_LEVEL, new_bat_level);
        if (new_bat_is_charging != bat_is_charging && callbacks[SENS_BAT_CHARGING] != NULL)
            callbacks[SENS_BAT_CHARGING](SENS_BAT_CHARGING, new_bat_is_charging);

        vbat_value = new_vbat_value;
        chrg_value = new_chrg_value;
        bat_level = new_bat_level;
        bat_is_charging = new_bat_is_charging;

        vTaskDelay(SENSORS_REFRESH_RATE);
    }

error:
    while (1) vTaskDelay(1000);
}

static void adc_calibration_init(void)
{
    esp_err_t ret;
    adc_calibrated = false;

    ret = esp_adc_cal_check_efuse(SENSORS_CALI_SCHEME);
    if (ret == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW("sens", "Calibration scheme not supported, skip software calibration");
    }
    else if (ret == ESP_ERR_INVALID_VERSION)
    {
        ESP_LOGW("sens", "eFuse not burnt, skip software calibration");
    }
    else if (ret == ESP_OK)
    {
        adc_calibrated = true;
        esp_adc_cal_characterize(ADC_UNIT_1, SENSORS_VBAT_ATT, ADC_WIDTH_BIT_DEFAULT, 0, &vbat_ch_chars);
        esp_adc_cal_characterize(ADC_UNIT_1, SENSORS_CHRG_ATT, ADC_WIDTH_BIT_DEFAULT, 0, &chrg_ch_chars);
    }
}

static inline float recalculate_vbat(float voltage)
{
    voltage = median_filter(vbat_filter_1, voltage);
    return kalman_filter(vbat_filter_2, voltage) * SENSORS_VBAT_MUL;
}

static inline float recalculate_chrg(float voltage)
{
    static const float rprog = 1100.0f;
    voltage = median_filter(chrg_filter_1, voltage);
    return ((kalman_filter(chrg_filter_2, voltage) / rprog) * 1100.0f) * SENSORS_CHRG_MUL;
}

static inline float recalculate_level(float voltage)
{
    return ((voltage - SENSORS_VBAT_MIN) / (SENSORS_VBAT_MAX - SENSORS_VBAT_MIN)) * 100;
}
