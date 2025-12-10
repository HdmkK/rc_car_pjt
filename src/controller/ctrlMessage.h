#ifndef __CTRL_MESSAGE__
#define __CTRL_MESSAGE__

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

int  ctrl_client_init(const char *ip, uint16_t port);
void ctrl_client_close(void);

// "구조체 그대로" 전송 (sizeof(Ctrl_Message) 바이트)
int  ctrl_send_message(const Ctrl_Message *msg);

// 편의 함수들
int  ctrl_send_track_start(void);
int  ctrl_send_track_stop(void);
int  ctrl_send_headlight(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
int  ctrl_send_laser(uint8_t on);

// 상태값 전체/부분 업데이트(외부에서 호출)
void drive_set_state(int16_t steering_deg, uint8_t gear, uint8_t speed);
void drive_set_steering(int16_t steering_deg);
void drive_set_gear(uint8_t gear);
void drive_set_speed(uint8_t speed);

// (선택) 현재 상태 조회
Drive_Payload drive_get_state(void);

#endif