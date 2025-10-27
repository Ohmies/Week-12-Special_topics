#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi_types.h"
#include "nvs_flash.h"
#include "esp_now.h"

static const char* TAG = "ESP_NOW_BIDIRECTIONAL";

// MAC Address ของอีกตัว (ต้องเปลี่ยนตามของจริง)
static uint8_t partner_mac[6] = {0x94, 0xB5, 0x55, 0xF8, 0x4B, 0xD8};

// ข้อมูลที่ส่ง/รับ
typedef struct {
    char sender_name[20];
    char message[200];
    uint32_t msg_id;
    bool is_ack;
} chat_message_t;

// Semaphore สำหรับรอ ACK
SemaphoreHandle_t ack_semaphore;
uint32_t message_counter = 0;

// Callback เมื่อส่งข้อมูลเสร็จ
void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "✅ Message delivered successfully");
    } else {
        ESP_LOGE(TAG, "❌ Failed to deliver message");
    }
}

// Callback เมื่อรับข้อมูล
void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    chat_message_t *recv_data = (chat_message_t*)data;
    
    if (recv_data->is_ack) {
        ESP_LOGI(TAG, "✅ ACK received for message ID: %lu", recv_data->msg_id);
        xSemaphoreGive(ack_semaphore);
    } else {
        ESP_LOGI(TAG, "📥 Received from %s (%02X:%02X:%02X:%02X:%02X:%02X):", 
                 recv_data->sender_name,
                 recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2], 
                 recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
        ESP_LOGI(TAG, "   � Message: %s", recv_data->message);
        ESP_LOGI(TAG, "   🔢 Message ID: %lu", recv_data->msg_id);
        
        // ตอบรับ (ACK)
        chat_message_t ack;
        strcpy(ack.sender_name, "ESP32_B");
        strcpy(ack.message, "Message received!");
        ack.msg_id = recv_data->msg_id;
        ack.is_ack = true;
        
        vTaskDelay(pdMS_TO_TICKS(100)); // หน่วงเวลาเล็กน้อย
        esp_now_send(recv_info->src_addr, (uint8_t*)&ack, sizeof(ack));
    }
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
    
    // เพิ่ม Peer
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, partner_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
    
    // สร้าง Semaphore สำหรับ ACK
    ack_semaphore = xSemaphoreCreateBinary();
    
    ESP_LOGI(TAG, "ESP-NOW bidirectional communication initialized");
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    init_espnow();
    
    // แสดง MAC Address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "📍 My MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    while (1) {
        // ส่งข้อความ
        chat_message_t msg;
        strcpy(msg.sender_name, "ESP32_A");
        sprintf(msg.message, "Hello ESP32_B! Message #%lu", message_counter);
        msg.msg_id = ++message_counter;
        msg.is_ack = false;
        
        ESP_LOGI(TAG, "📤 Sending message #%lu", msg.msg_id);
        esp_now_send(partner_mac, (uint8_t*)&msg, sizeof(msg));
        
        // รอ ACK
        if (xSemaphoreTake(ack_semaphore, pdMS_TO_TICKS(5000)) == pdTRUE) {
            ESP_LOGI(TAG, "ACK received, sending next message");
        } else {
            ESP_LOGE(TAG, "ACK timeout, retrying...");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // หน่วงเวลาเล็กน้อยก่อนส่งข้อความถัดไป
    }
}