#include "mic_sensor.h"

#define TAG_MIC "ADC_MIC_READ"



#define TIMER_BASE_CLK (80000000)  // 80MHz base clock for ESP32 timers
#define TIMER_DIVIDER 80 // 80MHz/80 = 1 MHz timer clock (1 tick is 1 microsecond)
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER) // 1,000,000 ticks per second
#define SAMPLE_RATE 16000
#define TIMER_INTERVAL_SEC (1.0 / SAMPLE_RATE) // 62.5 microsecond

volatile uint16_t audio_buffer[AUDIO_BUFFER_SIZE];
volatile int buffer_index = 0;
volatile bool buffer_ready = false;

QueueHandle_t mic_audio_queue = NULL;

static void setup_adc(void)
{
    // Configure ADC1 Channel 6 (GPIO34)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_12);
}

static void IRAM_ATTR timer_isr(void* arg)
{
    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0); // clear interrupt status register
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0); // re-enable interrupt
    int adc_read = adc1_get_raw(ADC1_CHANNEL_6); // get adc value

    if (buffer_index < 512)
    {
        audio_buffer[buffer_index] = adc_read;
        buffer_index++;
    }
    else if (buffer_index == 512)
    {
        buffer_ready = true;
        buffer_index = 0;
    }
}

static void setup_timer(void)
{
    timer_config_t config = 
    {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true
    };

    timer_init(TIMER_GROUP_0, TIMER_0, &config); // init timer
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0); // timer start value
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, TIMER_INTERVAL_SEC * TIMER_SCALE); // set alarm value in ticks
    timer_enable_intr(TIMER_GROUP_0, TIMER_0); // enable interrupt
    timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_isr, NULL, ESP_INTR_FLAG_IRAM, NULL); // tie to isr
    timer_start(TIMER_GROUP_0, TIMER_0); // start timer
}

static void audio_task(void* pvParameters)
{
    while (1)
    {
        // push buffer to queue
        if (buffer_ready)
        {
            buffer_ready = false;

            if (mic_audio_queue != NULL)
            {
                BaseType_t success = xQueueSend(mic_audio_queue, (void*) audio_buffer, portMAX_DELAY);
                // if (success == pdTRUE)
                // {
                //     ESP_LOGI(TAG_MIC, "Audio buffer sent to queue");
                // }
            }
        }
        // vTaskDelay(pdMS_TO_TICKS(5));
    }

    // vTaskDelete(NULL);
}

void audio_init(void)
{
    mic_audio_queue = xQueueCreate(8, sizeof(uint16_t) * AUDIO_BUFFER_SIZE); // queue of 8 buffers
    setup_adc();
    setup_timer();
    xTaskCreate(audio_task, "mic_sampling_task", 4096, NULL, 5, NULL); // create audio task with 4096 byte stack size
}