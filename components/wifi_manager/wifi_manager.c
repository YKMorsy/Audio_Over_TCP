#include "wifi_manager.h"

#define TAG_WIFI "WIFI"

#define MAX_WIFI_FAILURES 10
#define WIFI_SUCCESS 0x01
#define WIFI_FAILURE 0x02

static int wifi_connect_retry_num = 0;
static EventGroupHandle_t wifi_event_group;

/* Event handler for wifi events */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
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
static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) // on receiving and ip
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_WIFI, "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connect_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
    }
}


static esp_err_t connect_wifi(void)
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


esp_err_t init_wifi(void)
{    
    esp_err_t status = WIFI_FAILURE;
    status = connect_wifi();
    if (status != ESP_OK)
    {
        // print status code and info
        ESP_LOGE(TAG_WIFI, "Failed to connect to wifi");
        return status;        
    }

    return status;
}