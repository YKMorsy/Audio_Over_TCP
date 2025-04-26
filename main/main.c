#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/sockets.h"

#include "rom/ets_sys.h"
#include "driver/adc.h"

#include "sdkconfig.h"

/* DEFINES */
#define TAG_MIC "ADC_MIC_READ"
#define TAG_TH "TH"
#define TAG_WIFI "WIFI"

#define TEMP_HUM_DATA_BYTES 5
#define TEMP_HUM_DATA_BITS (TEMP_HUM_DATA_BYTES*8)

#define MAX_WIFI_FAILURES 10
#define WIFI_SUCCESS 1
#define WIFI_FAILURE 0
#define TCP_SUCCESS 1
#define TCO_FAILURE 0

/* GLOBALS */
gpio_num_t TEMP_HUM_PIN = 32;
int wifi_connect_retry_num = 0;

EventGroupHandle_t wifi_event_group;


void mic_in(void)
{
    // Configure ADC1 Channel 6 (GPIO34)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_12);

    while (1)
    {
        int adc_read = adc1_get_raw(ADC1_CHANNEL_6);
        ESP_LOGE(TAG_MIC, "ADC Reading (Channel 6 / GPIO34) %d", adc_read);
        vTaskDelay(pdMS_TO_TICKS(1000)); // delay for 1 sec
    }
}

esp_err_t th_await_pin_state(uint32_t timeout, int expected_pin_stae, uint32_t* duration)
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

esp_err_t fetch_data(uint8_t *data)
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

/* Event handler for wifi events */
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) // on wifi event start
    {
        ESP_LOGI(TAG_WIFI, "Connected to access point");
        esp_wifi_connect();
        // wifi_connect_retry_num = 0;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) // on wifi disconnect
    {
        if (wifi_connect_retry_num < MAX_WIFI_FAILURES)
        {
            ESP_LOGI(TAG_WIFI, "Reconnected to access point, try #%d", wifi_connect_retry_num);
            esp_wifi_connect();
            wifi_connect_retry_num++;
        }
        else
        {
            xEventGroupSetBits(wifi_event_group, WIFI_FAILURE);
        }
    }
}

// event handler on ip events
void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) // on receiving and ip
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_WIFI, "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connect_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
    }
}

esp_err_t connect_wifi(void)
{
    esp_err_t status = WIFI_FAILURE;

    if (esp_netif_init() != ESP_OK) // initialize network interface
    {
        ESP_LOGE(TAG_WIFI, "Failed to initialize network interface");
        return status;
    }
    if (esp_event_loop_create_default() != ESP_OK) // initialize default esp event loop
    {
        ESP_LOGE(TAG_WIFI, "Failed to initialize event loop");
        return status;
    }
    esp_netif_create_default_wifi_sta(); // create wifi station in the wifi driver

    // setup wifi station with default wifi configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) // initialize wifi driver
    {
        ESP_LOGE(TAG_WIFI, "Failed to initialize wifi driver");
        return status;
    }

    /* EVENT LOOP */
    wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t wifi_handler_event_instance;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        &wifi_handler_event_instance);

    esp_event_handler_instance_t ip_handler_event_instance;
    esp_event_handler_instance_register(IP_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &ip_event_handler,
                                        NULL,
                                        &ip_handler_event_instance);

    wifi_config_t wifi_config = 
    {
        .sta = 
        {
            .ssid = CONFIG_WIFI_SSID, // gotten from sdkconfig. make sure to add that into menuconfig using Kconfig 
            .password = CONFIG_WIFI_PASSWORD, // gotten from sdkconfig. make sure to add that into menuconfig using Kconfig 
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg =
            {
                .capable = true,
                .required = false
            },
        },
    };

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) // set wifi controller to be a station
    {
        ESP_LOGE(TAG_WIFI, "Failed to set wifi mode");
        return status;
    }
    if (esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK) // set wifi config
    {
        ESP_LOGE(TAG_WIFI, "Failed to set wifi config");
        return status;
    }
    if (esp_wifi_start() != ESP_OK) // start wifi, should trigger wifi event handler and ip
    {
        ESP_LOGE(TAG_WIFI, "Failed to start wifi");
        return status;
    }

    // wait until we get event bits
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, 0x03, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & 0b01)
    {
        ESP_LOGI(TAG_WIFI, "Connected to ap");
        status = WIFI_SUCCESS;
    }
    else
    {
        ESP_LOGI(TAG_WIFI, "Failed to connect to ap");
        status = WIFI_FAILURE;
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_handler_event_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_event_instance));
    vEventGroupDelete(wifi_event_group);

    status = ESP_OK;
    return status;

}

// connect to server and return the result
esp_err_t connect_tcp_server(void)
{
    // server information
    struct sockaddr_in serverInfo = {0};
    serverInfo.sin_family = AF_INET; // ipv4
    serverInfo.sin_addr.s_addr = inet_addr("192.168.0.193"); // 192.168.0.193
    serverInfo.sin_port = htons(1000); // port number

    // create tcp socket (default tcp protocol with IPv4)
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        ESP_LOGE(TAG_WIFI, "Failed to create a socket");
        return ESP_FAIL;
    }

    // connect socket (server receives SYN, send back a SYN-ACK, client sends ACK back)
    int sock_status = connect(sock, (struct sockaddr*)&serverInfo, sizeof(serverInfo));
    if (sock_status != 0) // check parameters of conenct
    {
        ESP_LOGE(TAG_WIFI, "Failed to connect to addr %s: errno=%d", inet_ntoa(serverInfo.sin_addr), errno);
        close(sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_WIFI, "Connected to TCP server.");

    // read server response

    return ESP_OK;
}

esp_err_t init_wifi(void)
{

	//initialize storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    esp_err_t status = WIFI_FAILURE;
    status = connect_wifi();
    if (status != ESP_OK)
    {
        // print status code and info
        ESP_LOGE(TAG_WIFI, "Failed to connect to wifi");
        return status;        
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    status = connect_tcp_server();
    if (status != ESP_OK)
    {
        // print status code and info
        ESP_LOGE(TAG_WIFI, "Failed to connect to tcp server");
        return status;        
    }

    return status;
}

void app_main(void)
{
    // mic_in();
    // temp_in();
    init_wifi();
}