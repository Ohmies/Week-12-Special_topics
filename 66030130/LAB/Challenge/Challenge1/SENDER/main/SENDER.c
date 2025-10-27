#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_now.h"

static const char *TAG = "ESP_NOW_SENDER";

// MAC Address ของ Receiver (ต้องเปลี่ยนตามของจริง)
static uint8_t receiver_mac[6] = {0x94, 0xB5, 0x55, 0xF8, 0x4B, 0xD8};

// ข้อมูลที่จะส่ง
typedef struct
{
    bool led_state;
    int brightness; // 0-255
    char command[20];
} led_control_t;

// Callback เมื่อส่งข้อมูลเสร็จ
void on_data_sent(const wifi_tx_info_t *wifi_tx, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        ESP_LOGI(TAG, "✅ Data sent successfully");
    }
    else
    {
        ESP_LOGE(TAG, "❌ Failed to send data");
    }
}

// ฟังก์ชันเริ่มต้น WiFi
void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialized");
}

// ฟังก์ชันเริ่มต้น ESP-NOW
void espnow_init(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));

    // เพิ่ม Peer (Receiver)
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, receiver_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
    ESP_LOGI(TAG, "ESP-NOW initialized and peer added");
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();
    espnow_init();

    led_control_t led_cmd;
    int counter = 0;

    ESP_LOGI(TAG, "🚀 ESP-NOW Sender started");

    while (1)
    {
        // เตรียมข้อมูลที่จะส่ง
        led_cmd.led_state = (counter % 2 == 0) ? true : false; // สลับสถานะ LED
        led_cmd.brightness = 128;                              // ความสว่างคงที่ หรือเปลี่ยนตามต้องการ
        strcpy(led_cmd.command, "SET_LED");

        // ส่งข้อมูล
        esp_err_t result = esp_now_send(receiver_mac, (uint8_t *)&led_cmd, sizeof(led_cmd));
        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "📤 Sending: LED %s, Brightness: %d", led_cmd.led_state ? "ON" : "OFF", led_cmd.brightness);
        }
        else
        {
            ESP_LOGE(TAG, "❌ Error sending data");
        }

        counter++;
        vTaskDelay(pdMS_TO_TICKS(2000)); // ส่งทุก 2 วินาที
    }
}