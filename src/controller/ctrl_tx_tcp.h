#ifndef __CTRL_TX_TCP_H__
#define __CTRL_TX_TCP_H__

#include "ctrl_protocol.h"

int  ctrl_client_init(const char *ip, uint16_t port);
void ctrl_client_close(void);

// "구조체 그대로" 전송 (sizeof(Ctrl_Message) 바이트)
int  ctrl_send_message(const Ctrl_Message *msg);

// 편의 함수들
int  ctrl_send_track_start(void);
int  ctrl_send_track_stop(void);
int  ctrl_send_headlight(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
int  ctrl_send_laser(uint8_t on);

#endif