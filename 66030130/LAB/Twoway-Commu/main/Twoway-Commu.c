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
#include "esp_timer.h"

static const char *TAG = "ESP_NOW_BIDIRECTIONAL";

// MAC Address ของอีกตัว (ต้องเปลี่ยนตามของจริง)
static uint8_t partner_mac[6] = {0x94, 0xB5, 0x55, 0xF8, 0x5D, 0x50};

// ข้อมูลที่ส่ง/รับ
typedef struct
{
    char device_name[50];
    char message[150];
    int counter;
    uint32_t timestamp;
} bidirectional_data_t;

// Callback เมื่อส่งข้อมูลเสร็จ
void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        ESP_LOGI(TAG, "✅ Message delivered successfully");
    }
    else
    {
        ESP_LOGE(TAG, "❌ Failed to deliver message");
    }
}

// Callback เมื่อรับข้อมูล
void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    bidirectional_data_t *recv_data = (bidirectional_data_t *)data;

    ESP_LOGI(TAG, "📥 Received from %s:", recv_data->device_name);
    ESP_LOGI(TAG, "   💬 Message: %s", recv_data->message);
    ESP_LOGI(TAG, "   🔢 Counter: %d", recv_data->counter);
    ESP_LOGI(TAG, "   ⏰ Timestamp: %lu", recv_data->timestamp);

    // ตอบกลับข้อมูล
    bidirectional_data_t reply_data;
    strcpy(reply_data.device_name, "Device_B");
    sprintf(reply_data.message, "Reply to message #%d - Thanks!", recv_data->counter);
    reply_data.counter = recv_data->counter;
    reply_data.timestamp = esp_timer_get_time() / 1000; // milliseconds

    vTaskDelay(pdMS_TO_TICKS(100)); // หน่วงเวลาเล็กน้อย
    esp_now_send(recv_info->src_addr, (uint8_t *)&reply_data, sizeof(reply_data));
}

// ฟังก์ชันเริ่มต้น WiFi และ ESP-NOW
void init_espnow(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    // เพิ่ม Peer
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, partner_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    ESP_LOGI(TAG, "ESP-NOW bidirectional communication initialized");
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    init_espnow();

    bidirectional_data_t send_data;
    int counter = 0;

    // แสดง MAC Address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "📍 My MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    while (1)
    {
        // ส่งข้อมูล
        strcpy(send_data.device_name, "Device_A");
        sprintf(send_data.message, "Hello! This is message number %d", counter);
        send_data.counter = counter++;
        send_data.timestamp = esp_timer_get_time() / 1000;

        ESP_LOGI(TAG, "📤 Sending message #%d", send_data.counter);
        esp_now_send(partner_mac, (uint8_t *)&send_data, sizeof(send_data));

        vTaskDelay(pdMS_TO_TICKS(5000)); // ส่งทุก 5 วินาที
    }
}