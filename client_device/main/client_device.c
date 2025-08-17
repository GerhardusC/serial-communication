#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/idf_additions.h"
#include "hal/gpio_types.h"
#include "esp_timer.h"
#include "esp_task.h"
#include "esp_log.h"

#define CLK 19
#define SELECT 21
#define MISO 22
#define OTHER 23

void wait_us_blocking(uint32_t micros_to_wait) {
    uint64_t micros_now_plus_delay = esp_timer_get_time() + micros_to_wait;
    while(micros_now_plus_delay > esp_timer_get_time()){}
}

uint16_t wait_for_clock_state(uint8_t expected_state){
	// Set as input pin to read from.
	// clock timeout 100000
	for(int i = 0; i < 10000; i += 2){
	// Wait one cycle apparently for jitter.
		wait_us_blocking(2);
		if(gpio_get_level(CLK) == expected_state){
		    return i;
		};
	}
	// ESP_LOGE("Err", "Pin wait timeout");
	return 0;
}

void send_message(int msg) {
	for (int i = 0; i < 32; i++) {
		// Read clock
		// Wait for falling edge.
		wait_for_clock_state(0);
		int val = (msg & (1 << i)) > 0 ? 1 : 0;
		// Send bit.
		gpio_set_level(MISO, val);
		ESP_LOGI("BIT_SENT", "%d", val);

		// Wait for clock to switch and bit to be read.
		wait_for_clock_state(1);

	}
}


void app_main(void)
{
	gpio_set_direction(2, GPIO_MODE_INPUT_OUTPUT);
	gpio_set_direction(MISO, GPIO_MODE_OUTPUT);
	gpio_set_direction(SELECT, GPIO_MODE_INPUT);
	gpio_set_direction(CLK, GPIO_MODE_INPUT);

	int i = 0;

	int message_sent = 0;
	while (1) {
		if(gpio_get_level(SELECT) == 0) {
			send_message(i);
			i++;
			vTaskDelay(1);
			while(gpio_get_level(SELECT) == 0) {
				vTaskDelay(1);
			};
		}
	}
}
