#include "dht.h"
#include "freertos/idf_additions.h"
#include "esp_timer.h"

#define DHT_METER_TIMER 2
#define DATA_LINE 19

// static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
// #define PORT_ENTER_CRITICAL() portENTER_CRITICAL(&mux)
// #define PORT_EXIT_CRITICAL() portEXIT_CRITICAL(&mux)

void wait_us_blocking(uint32_t micros_to_wait) {
    uint64_t micros_now_plus_delay = esp_timer_get_time() + micros_to_wait;
    while(micros_now_plus_delay > esp_timer_get_time()){}
}

void setup_thermometer() {
    gpio_reset_pin(DATA_LINE);
    gpio_pullup_en(DATA_LINE);
    gpio_set_direction(DATA_LINE, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(DATA_LINE, 0);
    gpio_set_level(DATA_LINE, 1);
}

uint16_t wait_for_pin_state(gpio_num_t pin, uint32_t timeout, uint8_t expected_state){
    // Set as input pin to read from.
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    for(int i = 0; i < timeout; i += DHT_METER_TIMER){
        // Wait one cycle apparently for jitter.
        wait_us_blocking(DHT_METER_TIMER);
        if(gpio_get_level(pin) == expected_state){
            return i;
        };
    }
    // ESP_LOGE("Err", "Pin wait timeout");
    return 0;
}

void read_temp_critical_section(struct Temp_reading *measurement){
    ESP_LOGI("INFO_LOG_START", "Log starting delay");
    // Low for 18 ms on sig line.
    gpio_set_level(DATA_LINE, 0);
    wait_us_blocking(18000);
    vTaskSuspendAll();
    gpio_set_level(DATA_LINE, 1);
    // Read response.
    // Phase 1: Wait 20-40 us for downpull.
    if(wait_for_pin_state(DATA_LINE, 40, 0) == 0){
        xTaskResumeAll();
        ESP_LOGE("ERR_THERMOMETER", "Phase 1 wait for pull down.");
        measurement->err = 1;
        gpio_set_level(DATA_LINE, 1);
        return;
    };
    // Phase 2: Wait for pull down by sensor
    if(wait_for_pin_state(DATA_LINE, 88, 1) == 0){
        xTaskResumeAll();
        ESP_LOGE("ERR_THERMOMETER", "Phase 2 wait for pull down by sensor.");
        measurement->err = 1;
        gpio_set_level(DATA_LINE, 1);
        return;
    };
    // Phase 3: Wait for pull down by sensor
    if(wait_for_pin_state(DATA_LINE, 88, 0) == 0){
        xTaskResumeAll();
        ESP_LOGE("ERR_THERMOMETER", "Phase 3 wait for pull down by sensor.");
        measurement->err = 1;
        gpio_set_level(DATA_LINE, 1);
        return;
    };
    uint8_t data[5] = { 0 };

    for(uint8_t i = 0; i < 40; i++){
        // measure low duration
        uint16_t base_dur = wait_for_pin_state(DATA_LINE, 72, 1);
        // measure high duration
        uint16_t bit_dur = wait_for_pin_state(DATA_LINE, 60, 0);

        uint8_t bit_index = i / 8;
        uint8_t num_byte = i % 8;
        if (num_byte == 0){
            data[bit_index] = 0;
        } 
        bool bit_value = bit_dur > base_dur;
        uint8_t current_byte = bit_value << (7 - num_byte);
        data[bit_index] = data[bit_index] | current_byte;
    };
    xTaskResumeAll();
    measurement->hum_sig = data[0];
    measurement->hum_dec = data[1];
    measurement->temp_sig = data[2];
    measurement->temp_dec = data[3];
    if(data[0] + data[1] + data[2] + data[3] == data[4]){
        measurement->err = 0;
    }
}

void read_temp(struct Temp_reading *measurement) {
    // Reset error state.
    measurement->err = 0;
    gpio_set_direction(DATA_LINE, GPIO_MODE_OUTPUT_OD);
    read_temp_critical_section(measurement);
    gpio_set_level(DATA_LINE, 1);
    return;
}
