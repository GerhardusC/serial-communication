#include <stdint.h>
// #include <stdio.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "hal/gpio_types.h"

#define CLK 19
#define SELECT 21
#define MISO 22
#define OTHER 23

#define MESSAGE_LENGTH 32

void wait_us_blocking(uint32_t micros_to_wait) {
    uint64_t micros_now_plus_delay = esp_timer_get_time() + micros_to_wait;
    while(micros_now_plus_delay > esp_timer_get_time()){}
}

void pulse_clock() {
	int old_level = gpio_get_level(CLK);
	gpio_set_level(CLK, !old_level);
	vTaskDelay(1);
}

int recv_message() {
	int msg = 0;

	// Send select signal to tell other device to start communicating.
	gpio_set_level(SELECT, 0);
	wait_us_blocking(80);
	gpio_set_level(CLK, 0);

	// Start pulsing the clock, and read MISO right before each clock pulse.
	for (int i = 0; i < MESSAGE_LENGTH; i++) {
		// Read val
		int current_bit = gpio_get_level(MISO);
		msg |= (current_bit << i);
		// Pulse clock
		pulse_clock();
		pulse_clock();
	}
	gpio_set_level(SELECT, 1);
	ESP_LOGI("MESSAGE RECEIVED", "%d", msg);
	return msg;
}

void app_main(void) {
	gpio_set_direction(SELECT, GPIO_MODE_OUTPUT);
	gpio_set_level(SELECT, 1);
	gpio_set_direction(CLK, GPIO_MODE_INPUT_OUTPUT);
	gpio_set_level(CLK, 1);
	gpio_set_direction(MISO, GPIO_MODE_INPUT);
	gpio_set_level(MISO, 1);

	while (1) {
		vTaskDelay(200);
		recv_message();
	}
}
