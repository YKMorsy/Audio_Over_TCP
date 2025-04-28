#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "esp_log.h"
#include "esp_err.h"
#include "lwip/sockets.h"

#include "mic_sensor.h" // pull in extern queue

esp_err_t tcp_server_init(void);

#endif // TCP_CLIENT_H