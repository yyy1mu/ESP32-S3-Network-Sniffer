#ifndef WIFI_HTTP_H
#define WIFI_HTTP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// WiFi配置
#define WIFI_SSID "Kwrt_2.4G"
#define WIFI_PASS "7778889654"

// HTTP服务器配置
#define HTTP_SERVER_IP "101.200.220.128"
#define HTTP_SERVER_PORT 3333

// 缓冲区大小
#define KEYBOARD_BUFFER_SIZE 160

void wifi_init_sta(void);
void wifi_wait_connected(void);
bool http_send_keyboard_data(const uint8_t* data, size_t len);
void keyboard_buffer_append(const uint8_t* data, size_t len);
void keyboard_buffer_init(void);

#endif
