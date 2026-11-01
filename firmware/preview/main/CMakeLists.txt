idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES nibbles_link nibbles_espnow nvs_flash esp_timer esp_lcd esp_driver_spi
                                     esp_driver_gpio esp_psram)
