#include <stdint.h>
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "hal/gpio_types.h"
#include "mqtt_client.h"

#include "secret.h"
#include "wifi.h"

const int TOP_BYTE     = 0b11111111 << 24;
const int TOP_MID_BYTE = 0b11111111 << 16;
const int BOT_MID_BYTE = 0b11111111 << 8;
const int BOT_BYTE     = 0b11111111;

#define CLK 19
#define SELECT 21
#define MISO 22
#define OTHER 23

#define MESSAGE_LENGTH 32
#define CHECKSUM_LENGTH 10

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

static esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = MY_BROKER_IP,
};

int check_measurement_err(struct Temp_reading *measurement) {
	if(measurement->temp_sig > 80) {
		return 1;
	}
	if(measurement->hum_sig > 110) {
		return 1;
	}
	return 0;
}

void post_messages_task(void *measurement) {
    struct Temp_reading* meas = (struct Temp_reading*) measurement;

    esp_mqtt_client_handle_t mqtthandle =  esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtthandle);

    uint8_t old_temp_sig = 0;
    uint8_t old_temp_dec = 0;
    uint8_t old_hum_sig = 0;
    uint8_t old_hum_dec = 0;

    while(1){
        if(check_measurement_err(meas) == 1){
		esp_mqtt_client_publish(mqtthandle, "/errors", "misread", 0, 1, 0);
		vTaskDelay(500);
		// Don't publish measurement if we misread.
		continue;
        } else {
            esp_mqtt_client_publish(mqtthandle, "/errors", "successful read", 0, 1, 0);
        }

        if(old_hum_sig != meas->hum_sig || old_hum_dec != meas->hum_dec){
            char humidity[16];
            sprintf(humidity, "%d.%d", meas->hum_sig, meas->hum_dec);
            esp_mqtt_client_publish(mqtthandle, "/home/humidity", humidity, 0, 1, 0);
            old_hum_sig = meas->hum_sig;
            old_hum_dec = meas->hum_dec;
            ESP_LOGI("MQTT", "Humidity published %d.%d", old_hum_sig, old_hum_dec);
        }
        if(old_temp_sig != meas->temp_sig || old_temp_dec != meas->temp_dec){
            char temperature[16];
            sprintf(temperature, "%d.%d", meas->temp_sig, meas->temp_dec);
            esp_mqtt_client_publish(mqtthandle, "/home/temperature", temperature, 0, 1, 0);
            old_temp_sig = meas->temp_sig;
            old_temp_dec = meas->temp_dec;
            ESP_LOGI("MQTT", "Temp published %d.%d", old_temp_sig, old_temp_dec);
        }
        vTaskDelay(500);
    }
}


void set_measurement_from_int(int msg, struct Temp_reading *measurement) {
	measurement->temp_sig = (TOP_BYTE & msg) >> 24;
	measurement->temp_dec = (TOP_MID_BYTE & msg) >> 16;
	measurement->hum_sig = (BOT_MID_BYTE & msg) >> 8;
	measurement->hum_dec = BOT_BYTE & msg;
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

/** 
 * If the message is zero there has been an error.
 * */
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

	// Receive Checksum
	int checksum = 0;
	for (int i = 0; i < CHECKSUM_LENGTH; i++) {
		// Read val
		int current_bit = gpio_get_level(MISO);
		checksum |= (current_bit << i);
		// Pulse clock
		gpio_set_level(CLK, 1);
		wait_us_blocking(10);
		gpio_set_level(CLK, 0);
		wait_us_blocking(10);
	}

	int top =	(TOP_BYTE & msg) >> 24;
	int top_mid =	(TOP_MID_BYTE & msg) >> 16;
	int bot_mid =	(BOT_MID_BYTE & msg) >> 8;
	int bot =	BOT_BYTE & msg;

	gpio_set_level(SELECT, 1);
	ESP_LOGI("MESSAGE RECEIVED", "%d", msg);

	if(top + top_mid + bot_mid + bot == checksum) {
		ESP_LOGI("CHECKSUM MATCHED", "Checksum: %d, Msg: %d", checksum, msg);
		return msg;
	}

	ESP_LOGI("CHECKSUM UNMATCHED", "On message: %d", msg);
	return 0;
}

void setup() {
	gpio_set_direction(SELECT, GPIO_MODE_OUTPUT);
	gpio_set_level(SELECT, 1);
	gpio_set_direction(CLK, GPIO_MODE_OUTPUT);
	gpio_set_level(CLK, 1);
	gpio_set_direction(MISO, GPIO_MODE_INPUT);
	gpio_set_level(MISO, 1);

	wifi_init_connection();
}

void receive_messages_task(void *measurement) {
	struct Temp_reading* meas = (struct Temp_reading*) measurement;
	while(1) {
		int msg = recv_message();
		if(msg != 0) {
			set_measurement_from_int(msg, meas);
			print_measurement(meas);
		} else {
			ESP_LOGI("NO MESSAGE", "No message: %d", msg);
		}
		vTaskDelay(300);
	}
}

void app_main(void) {
	setup();

	struct Temp_reading *measurement = malloc(sizeof(struct Temp_reading));

	xTaskCreate(post_messages_task, "Temp posting task", 4000, (void *) measurement, 1, NULL);
	xTaskCreate(receive_messages_task, "Temp receiving task", 4000, (void *) measurement, 2, NULL);

	while (1) {
		vTaskDelay(10000);
	}
}
