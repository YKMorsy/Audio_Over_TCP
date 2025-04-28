#include "tcp_client.h"

#define TAG_TCP "TCP_CLIENT"
#define PORT_NUM 1000

static int sock;

// connect to server and return the result
static esp_err_t connect_tcp_server(void)
{
    // server information
    struct sockaddr_in serverInfo = {0};
    serverInfo.sin_family = AF_INET; // ipv4
    serverInfo.sin_addr.s_addr = inet_addr("192.168.0.193"); // 192.168.0.193
    serverInfo.sin_port = htons(PORT_NUM); // port number

    // create tcp socket (default tcp protocol with IPv4)
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        ESP_LOGE(TAG_TCP, "Failed to create a socket");
        return ESP_FAIL;
    }

    // connect socket (server receives SYN, send back a SYN-ACK, client sends ACK back)
    int sock_status = connect(sock, (struct sockaddr*)&serverInfo, sizeof(serverInfo));
    if (sock_status != 0) // check parameters of conenct
    {
        ESP_LOGE(TAG_TCP, "Failed to connect to addr %s: errno=%d", inet_ntoa(serverInfo.sin_addr), errno);
        close(sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_TCP, "Connected to TCP server.");

    return ESP_OK;
}

static void tcp_send_task(void* pvParameters)
{
    uint16_t audio_buffer_tcp[AUDIO_BUFFER_SIZE];

    while (1)
    {
        // xQueueReceive(mic_audio_queue, audio_buffer_tcp, portMAX_DELAY);
        if (xQueueReceive(mic_audio_queue, audio_buffer_tcp, portMAX_DELAY))
        {
            // send audio_buffer with socket
            int bytes_sent = send(sock, (const void *) audio_buffer_tcp, AUDIO_BUFFER_SIZE * sizeof(uint16_t), 0);
            // if (bytes_sent < 0)
            // {
            //     ESP_LOGE(TAG_TCP, "Send failed: errno=%d", errno);
            //     break; 
            // }
            // else if (bytes_sent != sizeof(audio_buffer_tcp))
            // {
            //     ESP_LOGW(TAG_TCP, "Partial send: sent %d bytes", bytes_sent);
            // }
            // else
            // {
            //     ESP_LOGI(TAG_TCP, "Sent 1024 bytes");
            // }
        }
        // vTaskDelay(pdMS_TO_TICKS(5));
    }

    // close(sock);
    // vTaskDelete(NULL);
}

esp_err_t tcp_server_init(void)
{
    esp_err_t status = connect_tcp_server();
    if (status != ESP_OK)
    {
        // print status code and info
        ESP_LOGE(TAG_TCP, "Failed to connect to tcp server");
        return status;        
    }

    xTaskCreate(tcp_send_task, "tcp_send_task", 4096, NULL, 5, NULL); // task to send audio buffer to socket with 4096 byte stack

    return status;
}