#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_now.h"

static const char* TAG = "ESP_NOW_BROADCASTER";

// Broadcast Address (ส่งให้ทุกคน)
static uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// โครงสร้างข้อมูลที่ส่ง
typedef struct {
    char sender_id[20];
    char message[180];
    uint8_t message_type;  // 1=Info, 2=Command, 3=Alert
    uint8_t group_id;      // 0=All, 1=Group1, 2=Group2
    uint32_t sequence_num;
    uint32_t timestamp;
} broadcast_data_t;

static uint32_t sequence_counter = 0;

// Callback เมื่อส่งข้อมูลเสร็จ
void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "✅ Broadcast successful to: %02X:%02X:%02X:%02X:%02X:%02X", 
                 tx_info->des_addr[0], tx_info->des_addr[1], tx_info->des_addr[2], 
                 tx_info->des_addr[3], tx_info->des_addr[4], tx_info->des_addr[5]);
    } else {
        ESP_LOGW(TAG, "❌ Broadcast failed to: %02X:%02X:%02X:%02X:%02X:%02X", 
                 tx_info->des_addr[0], tx_info->des_addr[1], tx_info->des_addr[2], 
                 tx_info->des_addr[3], tx_info->des_addr[4], tx_info->des_addr[5]);
    }
}

// Callback เมื่อรับข้อมูลตอบกลับ
void on_data_recv(const struct esp_now_recv_info *recv_info, const uint8_t *data, int len) {
    ESP_LOGI(TAG, "📥 Reply from: %02X:%02X:%02X:%02X:%02X:%02X", 
             recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2], 
             recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
    
    broadcast_data_t *reply = (broadcast_data_t*)data;
    ESP_LOGI(TAG, "   Reply: %s", reply->message);
}

// ฟังก์ชันเริ่มต้น WiFi และ ESP-NOW
void init_espnow(void) {
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
    
    // เพิ่ม Broadcast Peer
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, broadcast_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
    
    ESP_LOGI(TAG, "ESP-NOW Broadcasting initialized");
}

// ฟังก์ชันส่งข้อมูล Broadcast
void send_broadcast(const char* message, uint8_t msg_type, uint8_t group_id) {
    broadcast_data_t broadcast_data;
    
    strcpy(broadcast_data.sender_id, "MASTER_001");
    strncpy(broadcast_data.message, message, sizeof(broadcast_data.message) - 1);
    broadcast_data.message_type = msg_type;
    broadcast_data.group_id = group_id;
    broadcast_data.sequence_num = ++sequence_counter;
    broadcast_data.timestamp = esp_timer_get_time() / 1000; // ms
    
    ESP_LOGI(TAG, "📡 Broadcasting [Type:%d, Group:%d]: %s", 
             msg_type, group_id, message);
    
    esp_err_t result = esp_now_send(broadcast_mac, (uint8_t*)&broadcast_data, sizeof(broadcast_data));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send broadcast: %s", esp_err_to_name(result));
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    init_espnow();
    
    // แสดง MAC Address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "📍 Broadcaster MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    ESP_LOGI(TAG, "🚀 ESP-NOW Broadcaster started");
    
    int message_count = 0;
    
    while (1) {
        // ส่งข้อความประเภทต่างๆ
        switch (message_count % 4) {
            case 0:
                send_broadcast("General announcement to all devices", 1, 0); // Info to All
                break;
            case 1:
                send_broadcast("Command for Group 1 devices", 2, 1); // Command to Group 1
                break;
            case 2:
                send_broadcast("Alert for Group 2 devices", 3, 2); // Alert to Group 2
                break;
            case 3:
                send_broadcast("Status update for all groups", 1, 0); // Info to All
                break;
        }
        
        message_count++;
        vTaskDelay(pdMS_TO_TICKS(5000)); // ส่งทุก 5 วินาที
    }
}