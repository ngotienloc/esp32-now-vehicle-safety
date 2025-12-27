idf_component_register(SRCS "gps.c" "main.c" "espnow.c"
                    PRIV_REQUIRES spi_flash esp_driver_uart esp_driver_gpio vfs nvs_flash esp_wifi esp_event esp_netif
                    INCLUDE_DIRS "." "include")
