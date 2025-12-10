#ifndef __DRIVE_TX_UDP_H__
#define __DRIVE_TX_UDP_H__

#include "ctrl_protocol.h"

// 상태값 전체/부분 업데이트(외부에서 호출)
void drive_set_state(int16_t steering_deg, uint8_t gear, uint8_t speed);
void drive_set_steering(int16_t steering_deg);
void drive_set_gear(uint8_t gear);
void drive_set_speed(uint8_t speed);

// (선택) 현재 상태 조회
Drive_Payload drive_get_state(void);

#endif