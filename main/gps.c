#include "gps.h"

static vehicle_state internal_gps_data; 
static QueueHandle_t uart_queue; 
static SemaphoreHandle_t gps_mutex; 
static void parse_vtg(char* line);

/*================== PROTOTYPE =====================*/
static float nmea_to_decimal(float raw);
void gps_task(void *arg);
void parse_rmc(char* line);

/*====================CONFIG========================*/
#define GPS_UART_PORT UART_NUM_1
#define GPS_BAURATE   9600
#define GPS_TX_PIN    GPIO_NUM_17
#define GPS_RX_PIN    GPIO_NUM_18

#define UART_RX_BUF_SIZE 2048
#define LINE_BUF_SIZE    128 

/*================UART INIT (DMA)==================*/
void gps_uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = GPS_BAURATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1, 
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE, 
        .source_clk = UART_SCLK_APB, 
    }; 

    uart_driver_install
    (
        GPS_UART_PORT, 
        UART_RX_BUF_SIZE, 
        0,
        20,
        &uart_queue,
        0
    );

    uart_param_config(GPS_UART_PORT, &cfg); 

    uart_set_pin
    (
        GPS_UART_PORT,
        GPS_TX_PIN,
        GPS_RX_PIN, 
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );
}

/*================Init GPS===============*/
void gps_init(void)
{ 
    gps_mutex  = xSemaphoreCreateMutex();
    memset(&internal_gps_data, 0, sizeof(internal_gps_data));

    gps_uart_init(); 
    
    /*=============Init Free Rtos for Task GPS==============*/
    xTaskCreatePinnedToCore(
        gps_task,       // Task function
        "gps_task",     // Name task
        4096,           // Stack size
        NULL,           // Use Parameter? 
        10,             // Priority
        NULL,           // Task handle
        0               // CORE 0
    ); 
}

void gps_task(void *arg)
{
    uart_event_t event; 
    uint8_t rxbuf[256]; 
    static int idx = 0; 
    static char line[LINE_BUF_SIZE]; 

    while(1)
    {
        if(xQueueReceive(uart_queue, &event, portMAX_DELAY))
        {
            if(event.type == UART_DATA){
                int len = uart_read_bytes(GPS_UART_PORT, rxbuf, event.size, 0);
                for(int i = 0; i < len; i++)
                {
                    char c = rxbuf[i]; 
                    if(c == '\n')
                    {
                        line[idx] = '\0'; 
                        idx = 0; 
                    if(strstr(line, "$GNRMC"))
                        parse_rmc(line);
                    else if(strstr(line, "$GNVTG"))
                        parse_vtg(line); 
                    }
                    else
                    {
                        if(idx < LINE_BUF_SIZE - 1)
                            line[idx++] = c; 
                    }
                    
                }
            }
        }
    }
}

/*===============Callibration Coordinates=================*/
static float nmea_to_decimal(float raw)
{
    int deg = (int)(raw / 100); 
    float min = raw - deg*100; 
    return deg + (min / 60.0f); 
}

void parse_rmc(char* line)
{
    char* token; 
    char *saveptr;
    int field = 0; 

    // Temp variable:
    char lat_dir = 'N', long_dir = 'E'; 
    
    // Split information when meet ','
    token = strtok_r(line, ",", &saveptr);
    while(token)
    {
        field++; 

        xSemaphoreTake(gps_mutex, portMAX_DELAY);
        if(field == 2 && strlen(token) >= 6)
        {
            snprintf(internal_gps_data.timestamp, sizeof(internal_gps_data.timestamp), "%.2s:%.2s:%.2s", token, token+2, token+4);
        }
        if(field == 3)  internal_gps_data.gps_data.status = token[0]; 
        if(field == 4)  internal_gps_data.gps_data.latitude = nmea_to_decimal(atof(token));
        if(field == 5)  lat_dir = token[0];
        if(field == 6)  internal_gps_data.gps_data.longtitude = nmea_to_decimal(atof(token)); 
        if(field == 7)  long_dir = token[0];
        xSemaphoreGive(gps_mutex);

        token = strtok_r(NULL, ",", &saveptr);
    }
    if(internal_gps_data.gps_data.status == 'A' && lat_dir && long_dir)
    {
        xSemaphoreTake(gps_mutex, portMAX_DELAY);
        if(lat_dir == 'S')  internal_gps_data.gps_data.latitude = -internal_gps_data.gps_data.latitude;
        if(long_dir == 'W') internal_gps_data.gps_data.longtitude = -internal_gps_data.gps_data.longtitude;
        xSemaphoreGive(gps_mutex);
        ESP_LOGI("GPS", "TIME: %s | LAT: %.6f | LON: %.6f",internal_gps_data.timestamp,internal_gps_data.gps_data.latitude, internal_gps_data.gps_data.longtitude); 
    }
    else{
        xSemaphoreTake(gps_mutex, portMAX_DELAY);
        internal_gps_data.gps_data.latitude = -1;
        internal_gps_data.gps_data.longtitude = -1;
        xSemaphoreGive(gps_mutex);
    }
}

static void parse_vtg(char* line)
{
    char* token; 
    char *saveptr;
    int field = 0; 

    //Split information when meet ","
    token = strtok_r(line, ",", &saveptr);
    while(token)
    {
        field++;

        xSemaphoreTake(gps_mutex, portMAX_DELAY);
        if(field == 7)  internal_gps_data.gps_data.speed = atof(token); 
        if(field == 9)  internal_gps_data.gps_data.mode_indicator = token[0]; 
        xSemaphoreGive(gps_mutex);

        token = strtok_r(NULL, ",", &saveptr);
    }
}

/*=====================Fill data to main========================*/
void gps_fill_data(vehicle_state *output_data){
    xSemaphoreTake(gps_mutex, portMAX_DELAY);
    output_data->gps_data.latitude = internal_gps_data.gps_data.latitude; 
    output_data->gps_data.longtitude = internal_gps_data.gps_data.longtitude; 
    output_data->gps_data.status = internal_gps_data.gps_data.status;
    output_data->gps_data.speed = internal_gps_data.gps_data.speed; 
    output_data->gps_data.mode_indicator = internal_gps_data.gps_data.mode_indicator; 
    xSemaphoreGive(gps_mutex);
}