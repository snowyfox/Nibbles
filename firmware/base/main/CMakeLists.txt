idf_component_register(
    SRCS main.c display.c controls.c power.c power_math.c keypad.c i2c_recover.c log_format.c logger.c rtc.c
    INCLUDE_DIRS .
    PRIV_INCLUDE_DIRS $ENV{IDF_PATH}/components/esp_driver_i2c
    REQUIRES nibbles_link nibbles_espnow nvs_flash esp_driver_gpio esp_driver_usb_serial_jtag esp_timer esp_lcd esp_driver_spi esp_driver_ledc esp_driver_i2c esp_driver_sdmmc sdmmc fatfs)
