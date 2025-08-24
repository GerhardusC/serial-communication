#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/idf_additions.h"
#include "hal/gpio_types.h"
#include "esp_log.h"

#include "shift_register.h"
#include "lcd_screen.h"
#include "utils.h"
#include "dht.h"

#define CLK 19
#define SELECT 21
#define MISO 22
#define OTHER 23

#define MESSAGE_LENGTH 32

uint16_t wait_for_clock_state(uint8_t expected_state){
	// Set as input pin to read from.
	// clock timeout 10000
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
	for (int i = 0; i < MESSAGE_LENGTH; i++) {
		// Read clock
		// Wait for falling edge.
		wait_for_clock_state(0);
		int val = (msg & (1 << i)) > 0 ? 1 : 0;
		// Send bit.
		gpio_set_level(MISO, val);

		// Wait for clock to switch and bit to be read.
		wait_for_clock_state(1);
	}
	ESP_LOGI("MSG_SENT", "%d", msg);
}

void listen_for_message_signal(int msg) {
	while (gpio_get_level(SELECT) == 1) {
		wait_us_blocking(2);
	}
	send_message(msg);
	vTaskDelay(1);
	while(gpio_get_level(SELECT) == 0) {
		vTaskDelay(1);
	};
}

/** 
 * Converts:
 * FROM: TT, tt, HH, hh
 * TO:   TTTTTTTTttttttttHHHHHHHHhhhhhhhh */
int convert_measurement_to_int(struct Temp_reading *measurement) {
	int top =	0b11111111 << 24;
	int top_mid =	0b11111111 << 16;
	int bot_mid =	0b11111111 << 8;
	int bot =	0b11111111;

	int temp_sig_mask = top     &	(measurement->temp_sig << 24);
	int temp_dec_mask = top_mid &	(measurement->temp_dec << 16);
	int hum_sig_mask  = bot_mid &	(measurement->hum_sig << 8);
	int hum_dec_mask  = bot     &	(measurement->hum_dec);

	return temp_sig_mask | temp_dec_mask | hum_sig_mask | hum_dec_mask;
}

void setup() {
	gpio_set_direction(MISO, GPIO_MODE_OUTPUT);
	gpio_set_direction(SELECT, GPIO_MODE_INPUT);
	gpio_set_direction(CLK, GPIO_MODE_INPUT);

	setup_shift_register();
	vTaskDelay(20);
	setup_screen();
}

void app_main(void) {
	setup();

	struct Temp_reading *measurement = malloc(sizeof(struct Temp_reading));

	read_temp(measurement);
	measurement->err = 0;
	measurement->hum_sig = 0;
	measurement->hum_dec = 0;
	measurement->temp_sig = 0;
	measurement->temp_dec = 0;

	while (1) {
		read_temp(measurement);
		if(!measurement->err){
			ESP_LOGI(
				"INFO_THERMOMETER_RESULTS",
				"Humidity: %d, Temp: %d",
				measurement->hum_sig,
				measurement->temp_sig
			);
		} else {
			ESP_LOGE("ERR_THERMOMETER_ERROR", "Failed to read temperature");
		}
		listen_for_message_signal(convert_measurement_to_int(measurement));
		// Display on screen right after msg sent, so we know we aren't busy.
		// TODO: Calculate length of measurement string
		char *temp_string = malloc(18);
		sprintf(temp_string, "Temp: %d,%d", measurement->temp_sig, measurement->temp_dec);

		char *hum_string = malloc(18);
		sprintf(hum_string, "Hum:  %d,%d", measurement->hum_sig, measurement->hum_dec);
		// Add dynamic length here.
		write_one_line(TOP, temp_string, 10);
		write_one_line(BOTTOM, hum_string, 10);
		free(temp_string);
		free(hum_string);
	}
}
