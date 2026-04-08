#include "wifi_http.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi_http";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
#define MAX_RETRY 10

// 键盘数据缓冲区
static uint8_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static size_t buffer_pos = 0;
static SemaphoreHandle_t buffer_mutex;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void wifi_wait_connected(void)
{
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASS);
    }
}

bool http_send_keyboard_data(const uint8_t* data, size_t len)
{
    char url[64];
    snprintf(url, sizeof(url), "http://%s:%d/keyboard", HTTP_SERVER_IP, HTTP_SERVER_PORT);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return false;
    }

    // 将二进制数据转换为hex字符串
    char* hex_data = malloc(len * 2 + 1);
    if (hex_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        esp_http_client_cleanup(client);
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        sprintf(hex_data + i * 2, "%02x", data[i]);
    }

    esp_http_client_set_header(client, "Content-Type", "text/plain");
    esp_http_client_set_post_field(client, hex_data, len * 2);

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);

    esp_http_client_cleanup(client);
    free(hex_data);

    if (err == ESP_OK && status_code == 200) {
        ESP_LOGI(TAG, "HTTP POST successful, status=%d", status_code);
        return true;
    } else {
        ESP_LOGE(TAG, "HTTP POST failed, err=%d status=%d", err, status_code);
        return false;
    }
}

void keyboard_buffer_init(void)
{
    buffer_mutex = xSemaphoreCreateMutex();
    if (buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create buffer mutex");
    }
}

void keyboard_buffer_append(const uint8_t* data, size_t len)
{
    if (buffer_mutex == NULL || data == NULL || len == 0) return;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);

    for (size_t i = 0; i < len; i++) {
        keyboard_buffer[buffer_pos++] = data[i];

        if (buffer_pos >= KEYBOARD_BUFFER_SIZE) {
            // 缓冲区满了，发送数据
            xSemaphoreGive(buffer_mutex);
            http_send_keyboard_data(keyboard_buffer, KEYBOARD_BUFFER_SIZE);
            xSemaphoreTake(buffer_mutex, portMAX_DELAY);
            buffer_pos = 0;
        }
    }

    xSemaphoreGive(buffer_mutex);
}
