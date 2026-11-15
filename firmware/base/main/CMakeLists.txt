# The screen depends on the board (main/Kconfig.projbuild): the 3.49's
# portrait LCD (display.c) or the 1.91's landscape AMOLED (display_191.c).
if(CONFIG_BASE_BOARD_AMOLED_191)
    set(display_src display_191.c)
else()
    set(display_src display.c)
endif()

idf_component_register(
    SRCS main.c ${display_src} ble_time.c controls.c power.c power_math.c keypad.c i2c_recover.c log_format.c logger.c rtc.c eye_thumb.c wled_palettes.c ota.c mirror_link.c ../../eyes/main/presets.c
    INCLUDE_DIRS .
    PRIV_INCLUDE_DIRS $ENV{IDF_PATH}/components/esp_driver_i2c
    REQUIRES bt nibbles_link nibbles_espnow nvs_flash esp_driver_gpio esp_driver_usb_serial_jtag esp_timer esp_lcd esp_driver_spi esp_driver_ledc esp_driver_i2c esp_driver_sdmmc sdmmc fatfs
             app_update esp_http_server esp_netif esp_wifi esp_event mbedtls bootloader_support esp_driver_uart esp_partition esp_app_format)
