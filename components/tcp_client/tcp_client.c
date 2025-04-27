#include "tcp_client.h"

#define TAG_TCP "TCP_CLIENT"

// connect to server and return the result
static esp_err_t connect_tcp_server(void)
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

    // read server response

    return ESP_OK;
}


static esp_err_t init_tcp(void)
{
    esp_err_t status = connect_tcp_server();
    if (status != ESP_OK)
    {
        // print status code and info
        ESP_LOGE(TAG_TCP, "Failed to connect to tcp server");
        return status;        
    }

    return status;
}