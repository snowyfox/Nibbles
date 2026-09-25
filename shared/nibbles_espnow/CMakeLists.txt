idf_component_register(
    SRCS src/nibbles_espnow.c
    INCLUDE_DIRS include
    REQUIRES nibbles_link
    PRIV_REQUIRES esp_wifi esp_event esp_timer)
