#ifndef ESPNOW_H
#define ESPNOW_H

#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "gps.h"

void espnow_init();
void wifi_init(); 
// Callback funtion to receive data on ESP_NOW
static void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len);
#endif