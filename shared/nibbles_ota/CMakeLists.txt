idf_component_register(
    SRCS src/ota.c src/ota_remote.c
    INCLUDE_DIRS include
    REQUIRES nibbles_link nibbles_espnow esp_common
    PRIV_REQUIRES app_update esp_http_server esp_netif esp_wifi esp_event mbedtls bootloader_support
                  esp_partition esp_app_format nvs_flash esp_timer)
