#include <stdio.h>
#include "espnow.h"

vehicle_state my_car;

void app_main(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }       
    gps_init();
    wifi_init();
    espnow_init(); 
    while(1)
    {
        gps_fill_data(&my_car);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
