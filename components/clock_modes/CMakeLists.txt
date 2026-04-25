idf_component_register(
    SRCS "mode_time.c" "mode_date.c" "mode_temperature.c" "mode_humidity.c"
         "mode_scoreboard.c" "mode_countdown.c" "mode_stopwatch.c" "mode_scroll.c"
    INCLUDE_DIRS "include"
    REQUIRES storage led_display sensors rtttl_player esp_timer wifi_manager
)
