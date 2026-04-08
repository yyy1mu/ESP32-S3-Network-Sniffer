// Import global project config
#include "config.h"

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000)); // At sleep in case of computer boot
    ESP_LOGI(LOG_TITLE, "Starting -MacroPassthrough- application");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    wifi_init_sta();
    wifi_wait_connected();

    // Initialize keyboard buffer
    keyboard_buffer_init();

    // Initialize SPI
    spi_init_slave_hid_receiver();
    spi_init_master_pc_sender();

    // Initialize hid multiplexer worker (keyboard/mouse passthrough)
    hid_init_multiplexer();

    // Initialize TinyUSB
    tud_user_initialization();

    // Leave main() in background
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}
