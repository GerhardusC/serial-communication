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
	gpio_set_direction(15, GPIO_MODE_OUTPUT);

	gpio_set_level(15, 0);

	setup_shift_register();
	vTaskDelay(20);
	setup_screen();
}

int len(char *string) {
	int i = 0;
	while (string[i] != '\0' && i < 32) { i++; };
	return i;
}

void display_on_screen_task(void *meas) {
	struct Temp_reading *measurement = (struct Temp_reading*) meas;
	while(1) {
		char *temp_string = calloc(18, 1);
		sprintf(temp_string, "Temp: %d,%d", measurement->temp_sig, measurement->temp_dec);

		char *hum_string = calloc(18, 1);
		sprintf(hum_string, "Hum:  %d,%d", measurement->hum_sig, measurement->hum_dec);

		// Add dynamic length here.
		write_one_line(TOP, temp_string, len(temp_string));
		write_one_line(BOTTOM, hum_string, len(hum_string));

		free(temp_string);
		free(hum_string);
		vTaskDelay(200);
	}
}

void read_temp_task(void *meas) {
	struct Temp_reading *measurement = (struct Temp_reading*) meas;

	while(1) {
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
		vTaskDelay(200);
	}
}

void listen_for_message_signal_task(void *meas) {
	struct Temp_reading *measurement = (struct Temp_reading*) meas;
	while (1) {
		listen_for_message_signal(convert_measurement_to_int(measurement));
	}
}

void app_main(void) {
	setup();

	struct Temp_reading measurement = {
		.temp_sig = 0,
		.temp_dec = 0,
		.hum_sig = 0,
		.hum_dec = 0,
		.err = 0,
	};

	xTaskCreate(display_on_screen_task, "Display On LCD", 4000, &measurement, 1, NULL);
	xTaskCreate(read_temp_task, "Read Temp", 4000, &measurement, 2, NULL);
	xTaskCreate(listen_for_message_signal_task, "Listen for message signal", 4000, &measurement, 3, NULL);

	while (1) {
		vTaskDelay(1000);
	}
}
