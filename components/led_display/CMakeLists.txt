idf_component_register(
    SRCS "led_display.c"
    INCLUDE_DIRS "include"
    REQUIRES led_strip driver freertos storage
)
