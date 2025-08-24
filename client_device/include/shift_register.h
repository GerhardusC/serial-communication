#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void clear_shift_register();
void setup_shift_register();
void toggle_shift_register_clock();
void push_u8_to_shift_register(uint8_t num);
void write_to_shift_register(uint8_t val);