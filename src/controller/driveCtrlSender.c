#define _POSIX_C_SOURCE 200809L
#include "ctrlMessage.h"

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int g_sock = -1;
static struct sockaddr_in g_dest;
static uint32_t g_period_ms = 50;

static pthread_t g_thread;
static int g_running = 0;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static Drive_Payload g_state = {0, 0, 0};

static void sleep_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec  = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms % 1000) * 1000000L);
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
}

static void build_packet(uint8_t out[4], const Drive_Payload *s) {
    // wire format: steering(int16 BE) + gear + speed => 4 bytes
    uint16_t be = htons((uint16_t)s->steering_deg); // 음수도 2's complement 그대로 전송됨
    out[0] = (uint8_t)((be >> 8) & 0xFF);
    out[1] = (uint8_t)(be & 0xFF);
    out[2] = s->gear;
    out[3] = s->speed;
}

static void *sender_thread(void *arg) {
    (void)arg;

    while (g_running) {
        Drive_Payload snap;
        pthread_mutex_lock(&g_lock);
        snap = g_state;               // 상태 스냅샷
        pthread_mutex_unlock(&g_lock);

        uint8_t pkt[4];
        build_packet(pkt, &snap);

        ssize_t n = sendto(g_sock, pkt, sizeof(pkt), 0,
                           (struct sockaddr *)&g_dest, sizeof(g_dest));
        if (n < 0) {
            // 너무 시끄러우면 로그 제거/레이트리밋 해도 됨
            perror("sendto");
        }

        sleep_ms(g_period_ms);
    }
    return NULL;
}

int drive_udp_start(const char *dest_ip, uint16_t dest_port, uint32_t period_ms) {
    if (!dest_ip || period_ms == 0) return -1;
    if (g_running) return 0; // 이미 실행 중이면 무시

    memset(&g_dest, 0, sizeof(g_dest));
    g_dest.sin_family = AF_INET;
    g_dest.sin_port   = htons(dest_port);
    if (inet_pton(AF_INET, dest_ip, &g_dest.sin_addr) != 1) return -1;

    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sock < 0) return -1;

    g_period_ms = period_ms;
    g_running = 1;

    if (pthread_create(&g_thread, NULL, sender_thread, NULL) != 0) {
        g_running = 0;
        close(g_sock);
        g_sock = -1;
        return -1;
    }
    return 0;
}

void drive_udp_stop(void) {
    if (!g_running) return;

    g_running = 0;
    pthread_join(g_thread, NULL);

    if (g_sock >= 0) {
        close(g_sock);
        g_sock = -1;
    }
}

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void drive_set_state(int16_t steering_deg, uint8_t gear, uint8_t speed) {
    steering_deg = clamp_i16(steering_deg, -180, 180);

    pthread_mutex_lock(&g_lock);
    g_state.steering_deg = steering_deg;
    g_state.gear = gear;
    g_state.speed = speed;
    pthread_mutex_unlock(&g_lock);
}

void drive_set_steering(int16_t steering_deg) {
    steering_deg = clamp_i16(steering_deg, -180, 180);
    pthread_mutex_lock(&g_lock);
    g_state.steering_deg = steering_deg;
    pthread_mutex_unlock(&g_lock);
}

void drive_set_gear(uint8_t gear) {
    pthread_mutex_lock(&g_lock);
    g_state.gear = gear;
    pthread_mutex_unlock(&g_lock);
}

void drive_set_speed(uint8_t speed) {
    pthread_mutex_lock(&g_lock);
    g_state.speed = speed;
    pthread_mutex_unlock(&g_lock);
}

Drive_Payload drive_get_state(void) {
    Drive_Payload s;
    pthread_mutex_lock(&g_lock);
    s = g_state;
    pthread_mutex_unlock(&g_lock);
    return s;
}


#include <stdio.h>
#include <unistd.h>

int main(void) {
    // 프로그램 시작과 동시에 송신 시작 (예: 20ms 주기)
    if (drive_udp_start("127.0.0.1", 8080, 20) != 0) {
        perror("drive_udp_start");
        return 1;
    }

    // 외부에서 상태 변경한다고 가정
    drive_set_state(0, 0, 0);     // 정지
    sleep(1);

    drive_set_state(10, 0, 80);   // 전진, 속도 80
    sleep(2);

    drive_set_steering(-30);      // 좌회전
    sleep(2);

    drive_set_state(0, 1, 60);    // 후진
    sleep(2);

    drive_udp_stop();
    return 0;
}

