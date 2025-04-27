#ifndef MIC_SENSOR_H
#define MIC_SENSOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "driver/timer.h"

void setup_timer(void);
void setup_adc(void);
void audio_task(void);

#endif // MIC_SENSOR_H