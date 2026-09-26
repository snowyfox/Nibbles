idf_component_register(
    SRCS main.c cartoon.c cartoon_eval.c audio.c audio_analysis.c motion.c motion_analysis.c eye.c render.c presets.c board.c link.c
    INCLUDE_DIRS .
    REQUIRES bsp_extra waveshare__esp32_s3_touch_amoled_1_75 waveshare__qmi8658
             esp_lcd esp_timer nvs_flash esp_driver_gpio esp_driver_uart nibbles_link nibbles_espnow)

# Let sqrtf/expf compile to FPU instructions instead of errno-setting library calls.
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-math-errno)

# Temporary cartoon evaluation firmware: idf.py -B build_eval -DEYES_CARTOON_EVAL=1
if(EYES_CARTOON_EVAL)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE EYES_CARTOON_EVAL=1)
endif()
