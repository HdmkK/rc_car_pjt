#ifndef __CTRL_MESSAGE_H__
#define __CTRL_MESSAGE_H__

#include <stdint.h>

#pragma pack(push, 1)

typedef struct {
	int16_t steering_deg; //조향 각도 -180 ~ 180(2byte)
	uint8_t gear; // 전진, 후진 여부(1byte)
	uint8_t speed; // 속력(1byte)
}Drive_Payload;

typedef struct { uint8_t _dummy; } Track_Start_Payload;
typedef struct { uint8_t _dummy; } Track_Stop_Payload;

typedef struct {
    uint8_t r;          // 0~255
    uint8_t g;          // 0~255
    uint8_t b;          // 0~255
    uint8_t brightness; // 0~100
} HeadLight_Ctrl_Payload;

typedef struct {
    uint8_t on; // 0=OFF, 1=ON
} Laser_Ctrl_Payload;

typedef union {
    Drive_Payload            drive_payload;
    Track_Start_Payload      track_start_payload;
    Track_Stop_Payload       track_stop_payload;
    HeadLight_Ctrl_Payload   headlight_ctrl_payload;
    Laser_Ctrl_Payload       laser_ctrl_payload;
} Payload;

typedef struct {
    uint8_t cmd;
    Payload payload;
} Ctrl_Message;

#pragma pack(pop)

// 명령 메시지 코드
enum {
    CMD_DRIVE       = 0x10,
    CMD_TRACK_START = 0x11,
    CMD_TRACK_STOP  = 0x12,
    CMD_HEADLIGHT   = 0x13,
    CMD_LASER       = 0x14,
};

#endif