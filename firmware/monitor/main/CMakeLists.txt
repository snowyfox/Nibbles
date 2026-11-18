idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES nibbles_link nibbles_espnow nibbles_ota nvs_flash esp_timer esp_lcd esp_driver_spi
                                     esp_driver_gpio esp_driver_usb_serial_jtag esp_psram)
