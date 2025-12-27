#include "espnow.h"

/*======================Global====================*/

/*Vehicle received status*/
static vehicle_state neighbor_car; 

/*My car status (another file)*/
extern vehicle_state my_car;

/*Mutex protects neighbor_car*/
static SemaphoreHandle_t espnow_mutex = NULL;

/*ESP-NOW data reception queue*/
static QueueHandle_t espnow_rx_queue = NULL; 


/*================== PROTOTYPE ==================*/
static void on_data_recv(const esp_now_recv_info_t *info,
                         const uint8_t *data,
                         int len);

void espnow_tx_task(void *pvParameter);
void espnow_rx_task(void *pvParameter);

/*==================Wifi Init====================*/
void wifi_init()
{
    ESP_ERROR_CHECK(esp_netif_init()); 
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI","WIFI STA Started"); 
}

/*==================ESP_NOW INIT=================*/
void espnow_init()
{
    /*Init NVS*/
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    /* Create mutex & queue */
    espnow_mutex = xSemaphoreCreateMutex();
    espnow_rx_queue = xQueueCreate(5, sizeof(vehicle_state));
    if (!espnow_mutex || !espnow_rx_queue) {
        ESP_LOGE("ESP_NOW", "Failed to create mutex or queue");
        return;
    }
    /* Init ESP-NOW */
    ESP_ERROR_CHECK(esp_now_init());

    /* Add broadcast peer */
    esp_now_peer_info_t peer_info = {};
    memset(&peer_info, 0, sizeof(esp_now_peer_info_t));
    memset(peer_info.peer_addr, 0xFF, 6); 
    peer_info.channel = 0;
    peer_info.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    /* Register callback */
    esp_now_register_recv_cb(on_data_recv);

    /* Create tasks */
      xTaskCreatePinnedToCore(
        espnow_tx_task,
        "espnow_tx_task",
        4096,
        NULL,
        10,
        NULL,
        0);

    xTaskCreatePinnedToCore(
        espnow_rx_task,
        "espnow_rx_task",
        4096,
        NULL,
        9,
        NULL,
        1);

    ESP_LOGI("ESP_NOW", "ESP-NOW init done");
}

/*=========================Transmit Task ===========================*/
void espnow_tx_task(void *pvParameter)
{
    uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; 
    while(1)
    {
        esp_err_t err = esp_now_send(broadcast_mac, (uint8_t *)&my_car, sizeof(vehicle_state));
        if (err != ESP_OK) {
            ESP_LOGE("ESP_NOW", "ESP-NOW send failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

/*================== RX PROCESS TASK ==================*/
void espnow_rx_task(void *pvParameter)
{
    vehicle_state rx;
    while (1)
    {
        if (xQueueReceive(espnow_rx_queue, &rx, portMAX_DELAY))
        {
            xSemaphoreTake(espnow_mutex, portMAX_DELAY);
            neighbor_car = rx;
            xSemaphoreGive(espnow_mutex);

            ESP_LOGI("ESP_NOW", "RX neighbor updated: ");
            ESP_LOGI("GPS", "LAT: %.6f | LON: %.6f",rx.gps_data.latitude, rx.gps_data.longtitude); 
        }
    }
}

/*=======================Callback for REV of ESP_NOW=====================*/
static void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len != sizeof(vehicle_state)) return;
    vehicle_state rx;
    memcpy(&rx, data, sizeof(vehicle_state));    
    /*Push to queue*/
    ESP_LOGI("ESP_NOW", "RX from MAC: %02X:%02X:%02X:%02X:%02X:%02X",
         info->src_addr[0], info->src_addr[1], info->src_addr[2],
         info->src_addr[3], info->src_addr[4], info->src_addr[5]);

    xQueueSendFromISR(espnow_rx_queue, &rx, NULL);

}

/*================== API GET NEIGHBOR ==================*/
bool espnow_get_neighbor(vehicle_state *out)
{
    if (!out) return false;

    if (xSemaphoreTake(espnow_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        *out = neighbor_car;
        xSemaphoreGive(espnow_mutex);
        return true;
    }
    return false;
}