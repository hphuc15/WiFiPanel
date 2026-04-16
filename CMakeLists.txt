idf_component_register(
    SRCS "src/WiFiManager_core.c" "src/WiFiManager_page.c" "src/WiFiManager_portal.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi nvs_flash esp_http_server esp_event
)