#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h> 
/*=============GPS Struct==============*/
typedef struct{
    float latitude; 
    float longtitude;
    char status; 
    float speed; 
    char mode_indicator;
}gps; 
/*===========MPU6050 Struct===========*/
typedef struct{
    float accX, accY, accZ; 
    float gyroX, gyroY, gyroZ; 
}mpu; 
/*===========Common struct============*/
typedef struct __attribute__((packed))
{
    uint16_t id; 
    gps gps_data;
    mpu mpu_data; 
    uint8_t rssi; 
    char timestamp[9]; 
}vehicle_state;
#endif
