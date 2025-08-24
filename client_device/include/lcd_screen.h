#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdint.h>
#include "shift_register.h"

enum Line16x2 {
    TOP,
    BOTTOM
};

void send_cmd(int cmd);
void write_data(uint8_t data);
void setup_screen();
void write_string(char *string);
void write_one_line(enum Line16x2 line, char *string, uint8_t len);
