#define _POSIX_C_SOURCE 200809L
#include "ctrl_tx_tcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int g_fd = -1;
static struct sockaddr_in g_addr;

static int send_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t*)buf;
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n > 0) { sent += (size_t)n; continue; }
        if (n == 0) return -1;
        if (errno == EINTR) continue;
        return -1;
    }
    return 0;
}

static int ensure_connected(void) {
    if (g_fd >= 0) return 0;

    g_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_fd < 0) return -1;

    // 선택: Nagle off (지연 줄이기)
    int one = 1;
    setsockopt(g_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    // 선택: keepalive
    setsockopt(g_fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));

    if (connect(g_fd, (struct sockaddr *)&g_addr, sizeof(g_addr)) < 0) {
        close(g_fd);
        g_fd = -1;
        return -1;
    }
    return 0;
}

int ctrl_client_init(const char *ip, uint16_t port) {
    memset(&g_addr, 0, sizeof(g_addr));
    g_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &g_addr.sin_addr) != 1) return -1;
    g_addr.sin_port   = htons(port);
    return ensure_connected(); 
}

void ctrl_client_close(void) {
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
}

int ctrl_send_message(const Ctrl_Message *msg) {
    if (!msg) return -1;
    if (ensure_connected() != 0) return -1;

    // 구조체 그대로 전송 (pack(1)로 패딩 제거된 레이아웃)
    if (send_all(g_fd, msg, sizeof(Ctrl_Message)) != 0) {
        ctrl_client_close(); // 끊겼으면 닫아두고 다음 호출 때 재연결 시도
        return -1;
    }
    return 0;
}

// ---- 편의 함수들 ----

int ctrl_send_track_start(void) {
    Ctrl_Message m;
    memset(&m, 0, sizeof(m));          // union 쓰레기 바이트 제거 필수
    m.cmd = CMD_TRACK_START;
    m.payload.track_start_payload._dummy = 0;
    return ctrl_send_message(&m);
}

int ctrl_send_track_stop(void) {
    Ctrl_Message m;
    memset(&m, 0, sizeof(m));
    m.cmd = CMD_TRACK_STOP;
    m.payload.track_stop_payload._dummy = 0;
    return ctrl_send_message(&m);
}

int ctrl_send_headlight(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    Ctrl_Message m;
    memset(&m, 0, sizeof(m));
    m.cmd = CMD_HEADLIGHT;
    m.payload.headlight_ctrl_payload = (HeadLight_Ctrl_Payload){ r, g, b, brightness };
    return ctrl_send_message(&m);
}

int ctrl_send_laser(uint8_t on) {
    Ctrl_Message m;
    memset(&m, 0, sizeof(m));
    m.cmd = CMD_LASER;
    m.payload.laser_ctrl_payload.on = (uint8_t)(on ? 1 : 0);
    return ctrl_send_message(&m);
}

// int ctrl_send_drive(int16_t speed, int16_t steer) {
//     Ctrl_Message m;
//     memset(&m, 0, sizeof(m));
//     m.cmd = CMD_DRIVE;
//     m.payload.drive_payload.speed = speed;
//     m.payload.drive_payload.steer = steer;
//     return ctrl_send_message(&m);
// }




#include <stdio.h>

int main(void) {
    if (ctrl_client_init("127.0.0.1", 8080) != 0) {
        perror("ctrl_client_init");
        return 1;
    }

    ctrl_send_track_start();
    ctrl_send_headlight(255, 80, 0, 50);
    ctrl_send_laser(1);
    //ctrl_send_drive(120, -10);
    ctrl_send_track_stop();

    ctrl_client_close();
    return 0;
}