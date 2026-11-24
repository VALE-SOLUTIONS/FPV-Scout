idf_component_register(SRCS "cyd_c5.c" "cyd_font_smooth.c" "cyd_indicator.c" "cyd_wallpaper.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES esp_driver_spi esp_driver_gpio
    PRIV_REQUIRES nvs_flash esp_driver_rmt esp_timer esp_driver_sdspi fatfs esp_psram)
