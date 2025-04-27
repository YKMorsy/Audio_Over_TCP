#include "wifi_manager.h"
#include "mic_sensor.h"
#include "nvs_flash_comp.h"

/*
1. Capture audio
    up to 20 kHz electret microphone
    12 bit ADC width
    16 kHz sampling rate -> 62.5 microsecond interrupts

2. Buffer data
    Put samples in small ring buffer buffer (512 samples = 32ms)
3. Compress audio (maybe)
4. Send over network
    TCP or UDP (currently over slower TCP)

*/

/* stream mic over tcp */
void app_main(void)
{
    setup_adc();
    setup_timer();
    audio_task();
    // mic_in();
    // // temp_in();
    // nvs_flash_comp_init();

    // init_wifi();

}