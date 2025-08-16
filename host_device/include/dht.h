#include "esp_log.h"
#include "driver/gpio.h"
#include "utils.h"

struct Temp_reading {
    uint16_t temp_sig;
    uint16_t temp_dec;
    uint16_t hum_sig;
    uint16_t hum_dec;
    uint8_t err;
};

uint16_t wait_for_pin_state(gpio_num_t pin, uint32_t timeout, uint8_t expected_state);

void read_temp(struct Temp_reading *measurement);
void setup_thermometer();
