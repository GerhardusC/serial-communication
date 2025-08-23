#include <stdint.h>
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

struct Temp_reading {
    uint16_t temp_sig;
    uint16_t temp_dec;
    uint16_t hum_sig;
    uint16_t hum_dec;
};

void set_measurement_from_int(int msg, struct Temp_reading *measurement) {
	int top =	0b11111111 << 24;
	int top_mid =	0b11111111 << 16;
	int bot_mid =	0b11111111 << 8;
	int bot =	0b11111111;

	measurement->temp_sig = (top & msg) >> 24;
	measurement->temp_dec = (top_mid & msg) >> 16;
	measurement->hum_sig = (bot_mid & msg) >> 8;
	measurement->hum_dec = bot & msg;
}

void print_measurement(struct Temp_reading *measurement) {
	ESP_LOGI(
		"MEASUREMENT",
		"\nTemp: %d,%d\nHum:  %d,%d",
		measurement->temp_sig,
		measurement->temp_dec,
		measurement->hum_sig,
		measurement->hum_dec
	);
}

int recv_message() {
	int msg = 0;

	// Send select signal to tell other device to start communicating.
	gpio_set_level(SELECT, 0);
	wait_us_blocking(10);
	gpio_set_level(CLK, 0);

	// Start pulsing the clock, and read MISO right before each clock pulse.
	for (int i = 0; i < MESSAGE_LENGTH; i++) {
		// Read val
		int current_bit = gpio_get_level(MISO);
		msg |= (current_bit << i);
		// Pulse clock
		gpio_set_level(CLK, 1);
		wait_us_blocking(10);
		gpio_set_level(CLK, 0);
		wait_us_blocking(10);
	}
	gpio_set_level(SELECT, 1);
	ESP_LOGI("MESSAGE RECEIVED", "%d", msg);
	return msg;
}

void app_main(void) {
	gpio_set_direction(SELECT, GPIO_MODE_OUTPUT);
	gpio_set_level(SELECT, 1);
	gpio_set_direction(CLK, GPIO_MODE_OUTPUT);
	gpio_set_level(CLK, 1);
	gpio_set_direction(MISO, GPIO_MODE_INPUT);
	gpio_set_level(MISO, 1);

	struct Temp_reading *measurement = malloc(sizeof(struct Temp_reading));

	while (1) {
		vTaskDelay(200);
		int msg = recv_message();
		set_measurement_from_int(msg, measurement);
		print_measurement(measurement);
	}
}
