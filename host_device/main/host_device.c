#include <stdint.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"

#define CLK 1
#define SELECT 2
#define MISO 3

#define MESSAGE_LENGTH 32

void app_main(void) {
	gpio_set_direction(SELECT, GPIO_MODE_OUTPUT);
	gpio_set_level(SELECT, 1);
	gpio_set_direction(CLK, GPIO_MODE_INPUT_OUTPUT);
	gpio_set_level(CLK, 1);
	gpio_set_direction(MISO, GPIO_MODE_INPUT);
	gpio_set_level(MISO, 1);

	while (1) {

	}
}

void pulse_clock() {
	int old_level = gpio_get_level(CLK);
	gpio_set_level(CLK, !old_level);
	vTaskDelay(1);
}

int send_message(uint32_t msg) {
	
}

int recv_message() {
	int msg = 0;

	// Send select signal to tell other device to start communicating.
	gpio_set_level(SELECT, 0);

	// Start pulsing the clock, and read MISO right before each clock pulse.
	for (int i = 0; i < 32; i++) {
		// Read val
		int current_bit = gpio_get_level(MISO);
		msg |= (current_bit << i);

		// Pulse clock
		pulse_clock();
		pulse_clock();
	}
	gpio_set_level(SELECT, 1);
	return msg;
}

