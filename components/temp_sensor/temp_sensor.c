#include "temp_sensor.h"

#define TAG_TH "TH"

#define TEMP_HUM_DATA_BYTES 5
#define TEMP_HUM_DATA_BITS (TEMP_HUM_DATA_BYTES*8)

static gpio_num_t TEMP_HUM_PIN = 32;

static esp_err_t th_await_pin_state(uint32_t timeout, int expected_pin_stae, uint32_t* duration)
{
    gpio_set_direction(TEMP_HUM_PIN, GPIO_MODE_INPUT);
    for (uint32_t i = 0; i < timeout; i += 2)
    {
        ets_delay_us(2);
        if (gpio_get_level(TEMP_HUM_PIN) == expected_pin_stae)
        {
            if (duration) *duration = i;
            return ESP_OK;
        }
    }

    return ESP_ERR_TIMEOUT;
}


static esp_err_t fetch_data(uint8_t *data)
{
    // pull signal low to initiate read squence w delay 
    gpio_set_direction(TEMP_HUM_PIN, GPIO_MODE_OUTPUT_OD); // MUST USE PULL-UP RESISTOR
    gpio_set_level(TEMP_HUM_PIN, 0);
    ets_delay_us(20000);
    gpio_set_level(TEMP_HUM_PIN, 1);

    // check if data is ready
    if (th_await_pin_state(40, 0, NULL) != ESP_OK) ESP_LOGE(TAG_TH,  "Initialization error, phase B");
    if (th_await_pin_state(88, 1, NULL) != ESP_OK) ESP_LOGE(TAG_TH,  "Initialization error, phase C");
    if (th_await_pin_state(88, 0, NULL) != ESP_OK) ESP_LOGE(TAG_TH,  "Initialization error, phase D");
    
    // read data using durations of low and high
    uint32_t low_duration;
    uint32_t  high_duration;
    for (int i = 0; i < TEMP_HUM_DATA_BITS; i++)
    {
        if (th_await_pin_state(65, 1, &low_duration) != ESP_OK) ESP_LOGE(TAG_TH,  "Low bit timeout");
        if (th_await_pin_state(75, 0, &high_duration) != ESP_OK) ESP_LOGE(TAG_TH,  "High bit timeout");
        uint8_t b = i/8;
        uint8_t m = i%8;
        data[b] |= (high_duration > low_duration) << (7 - m);
    }

    // check sum
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
    {
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

void temp_in(void)
{
    float humidity;
    float temperature;

    // get data buffer
    uint8_t data[TEMP_HUM_DATA_BYTES] = {}; // 5 bytes of data
    esp_err_t result = fetch_data(data);

    if (result != ESP_OK)
    {
        ESP_LOGE(TAG_TH,  "Fetch data error");
        return;
    }

    // get humidity and temperature from data
    humidity = (float)(data[0] * 10)/10;
    temperature = (float)(data[2] * 10)/10;

    ESP_LOGI(TAG_TH,  "Sensor data: humidity=%f, temp=%f deg C", humidity, temperature);

}