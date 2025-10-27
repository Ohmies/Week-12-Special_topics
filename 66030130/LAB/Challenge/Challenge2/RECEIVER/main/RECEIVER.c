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
#include <esp_timer.h>

static const char *TAG = "ESP_NOW_RECEIVER";

// โครงสร้างข้อมูลที่รับ (ต้องเหมือนกับ Sender)
typedef struct {
    float temperature;
    float humidity;
    int light_level;
    char sensor_id[10];
    uint32_t timestamp;
} sensor_data_t;

// Callback เมื่อรับข้อมูล
void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    sensor_data_t *recv_data = (sensor_data_t *)data;

    ESP_LOGI(TAG, "📥 Received from: %02X:%02X:%02X:%02X:%02X:%02X",
             recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
             recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);

    ESP_LOGI(TAG, "📨 Temperature: %.2f", recv_data->temperature);
    ESP_LOGI(TAG, "� Humidity: %.2f", recv_data->humidity);
    ESP_LOGI(TAG, "💡 Light Level: %d", recv_data->light_level);
    ESP_LOGI(TAG, "� Sensor ID: %s", recv_data->sensor_id);
    ESP_LOGI(TAG, "⏰ Timestamp: %u", recv_data->timestamp);
    ESP_LOGI(TAG, "📦 Data Length: %d bytes", len);
    ESP_LOGI(TAG, "--------------------------------");
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
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    ESP_LOGI(TAG, "ESP-NOW initialized and ready to receive");
}

// ฟังก์ชันแสดง MAC Address
void print_mac_address(void)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "📍 My MAC Address: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "⚠️  Copy this MAC to Sender code!");
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();
    print_mac_address();
    espnow_init();

    ESP_LOGI(TAG, "🎯 ESP-NOW Receiver started - Waiting for data...");

    // Receiver จะทำงานใน callback ไม่ต้องมี loop
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}