idf_component_register(
    SRCS main.c display.c
    INCLUDE_DIRS .
    REQUIRES nibbles_link nibbles_espnow nvs_flash esp_driver_gpio esp_driver_usb_serial_jtag esp_timer esp_lcd esp_driver_spi esp_driver_ledc esp_driver_i2c)
