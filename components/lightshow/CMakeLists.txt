idf_component_register(
    SRCS "lightshow.c" "lightshow_fx.c" "spectrum.c"
    INCLUDE_DIRS "include"
    REQUIRES storage led_display sensors freertos
)
