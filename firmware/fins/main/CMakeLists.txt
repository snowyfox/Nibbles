idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES nibbles_link nibbles_espnow nvs_flash esp_adc esp_timer
                                     esp_driver_usb_serial_jtag)
