idf_component_register(
    SRCS "web_server.c" "handlers_clock.c" "handlers_date_temp_humi.c"
         "handlers_scores_cd_ls.c" "handlers_scroll_spec.c" "handlers_system.c"
    INCLUDE_DIRS "include"
    REQUIRES storage led_display sensors clock_modes lightshow rtttl_player
             esp_http_server spiffs app_update freertos esp_timer
             esp_wifi esp_netif mdns
)
