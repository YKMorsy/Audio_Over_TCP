#ifndef MIC_SENSOR_H
#define MIC_SENSOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "driver/timer.h"

#define AUDIO_BUFFER_SIZE 512

extern QueueHandle_t mic_audio_queue; // declared here, defined in mic_sensor.c and used in tcp_server

void audio_init(void);

#endif // MIC_SENSOR_H