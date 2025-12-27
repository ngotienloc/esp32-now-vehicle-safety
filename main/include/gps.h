#ifndef GPS_H
#define GPS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include  "common_types.h"
// Struct use in common__types.h so we dont't need to write them again
/*=============Public API=============*/
void gps_init(void); 
void gps_uart_init(void); 
void gps_fill_data(vehicle_state *outdata); 
#endif