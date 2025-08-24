#include "shift_register.h"

#define SHIFT_REGISTER_RESET 13
#define SHIFT_REGISTER_A 4
#define SHIFT_REGISTER_CLOCK 14

void toggle_shift_register_clock() {
    gpio_set_level(SHIFT_REGISTER_CLOCK, 1);
    gpio_set_level(SHIFT_REGISTER_CLOCK, 0);
}

void write_to_shift_register(uint8_t val) {
    // Set data pin to val
    gpio_set_level(SHIFT_REGISTER_A, val);
    // Tick clock
    toggle_shift_register_clock();
    // reset data pin
    gpio_set_level(SHIFT_REGISTER_A, 0);
}

void push_u8_to_shift_register(uint8_t num){
    for(uint8_t i = 0; i < 8; i++){
        uint8_t val = num & 1 << (7 - i);
        write_to_shift_register(val);
    }
}

void setup_shift_register() {
    gpio_reset_pin(SHIFT_REGISTER_RESET);
    gpio_set_direction(SHIFT_REGISTER_RESET, GPIO_MODE_OUTPUT);
    gpio_set_level(SHIFT_REGISTER_RESET, 1);

    gpio_reset_pin(SHIFT_REGISTER_A);
    gpio_set_direction(SHIFT_REGISTER_A, GPIO_MODE_OUTPUT);

    gpio_reset_pin(SHIFT_REGISTER_CLOCK);
    gpio_set_direction(SHIFT_REGISTER_CLOCK, GPIO_MODE_OUTPUT);
}

void clear_shift_register() {
    gpio_reset_pin(SHIFT_REGISTER_RESET);
    gpio_set_direction(SHIFT_REGISTER_RESET, GPIO_MODE_OUTPUT);
    gpio_set_level(SHIFT_REGISTER_RESET, 1);
    gpio_set_level(SHIFT_REGISTER_RESET, 0);
    gpio_set_level(SHIFT_REGISTER_RESET, 1);
}
