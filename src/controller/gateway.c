#include "ctrl_protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>      // O_NONBLOCK
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// SocketCAN
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

// =====================
// CAN ID 매핑 (필요시 수정)
// =====================
#define CAN_ID_DRIVE        0x120
#define CAN_ID_TRACK_START  0x121
#define CAN_ID_TRACK_STOP   0x122
#define CAN_ID_HEADLIGHT    0x123
#define CAN_ID_LASER        0x124

static volatile sig_atomic_t g_running = 1;

static void on_sigint(int sig) {
    (void)sig;
    g_running = 0;
}



// steering: big-endian 2바이트 -> host int16
static int16_t parse_i16_be(const uint8_t b[2]) {
    int16_t u = ((uint8_t)b[0] << 8) | (uint8_t)b[1];
    printf("host : %d\n", u);
    return (int16_t)u;
}


// host int16 -> big-endian 2바이트
static void write_i16_be(uint8_t out[2], int16_t v) {
    uint16_t u = (uint16_t)v;
    out[0] = (uint8_t)((u >> 8) & 0xFF);
    out[1] = (uint8_t)(u & 0xFF);
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    return 0;
}

static int open_can_socket(const char *ifname) {
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("socket(PF_CAN)");
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);

    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl(SIOCGIFINDEX)");
        close(s);
        return -1;
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind(AF_CAN)");
        close(s);
        return -1;
    }

    return s;
}

static int open_udp_listener(int port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket(udp)"); return -1; }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind(udp)");
        close(fd);
        return -1;
    }

    // 핵심: UDP는 논블로킹으로 설정해서 "드레인(EAGAIN까지)" 가능하게 함
    if (set_nonblocking(fd) < 0) {
        perror("fcntl(O_NONBLOCK udp)");
        close(fd);
        return -1;
    }

    return fd;
}

static int open_tcp_listener(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket(tcp)"); return -1; }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind(tcp)");
        close(fd);
        return -1;
    }

    if (listen(fd, 16) < 0) {
        perror("listen(tcp)");
        close(fd);
        return -1;
    }
    return fd;
}

static uint32_t can_id_from_cmd(uint8_t cmd) {
    switch (cmd) {
        case CMD_DRIVE:       return CAN_ID_DRIVE;
        case CMD_TRACK_START: return CAN_ID_TRACK_START;
        case CMD_TRACK_STOP:  return CAN_ID_TRACK_STOP;
        case CMD_HEADLIGHT:   return CAN_ID_HEADLIGHT;
        case CMD_LASER:       return CAN_ID_LASER;
        default:              return 0; // invalid
    }
}

// Ctrl_Message(논리) -> CAN data[5] (cmd + payload4)
// drive는 steering을 BE로 넣는 것을 기본으로 함
static void encode_ctrl_to_can_data(uint8_t out5[5], const Ctrl_Message *m, int drive_steering_is_host_endian) {
    memset(out5, 0, 5);
    out5[0] = m->cmd;

    switch (m->cmd) {
        case CMD_DRIVE: {
            int16_t steering = m->payload.drive_payload.steering_deg;
            (void)drive_steering_is_host_endian; // 현재 구현에서는 host값 기준으로만 처리
            write_i16_be(&out5[1], steering);
            out5[3] = m->payload.drive_payload.gear;
            out5[4] = m->payload.drive_payload.speed;
            break;
        }
        case CMD_HEADLIGHT: {
            const HeadLight_Ctrl_Payload *p = &m->payload.headlight_ctrl_payload;
            out5[1] = p->r;
            out5[2] = p->g;
            out5[3] = p->b;
            out5[4] = p->brightness;
            break;
        }
        case CMD_LASER: {
            const Laser_Ctrl_Payload *p = &m->payload.laser_ctrl_payload;
            out5[1] = p->on;
            break;
        }
        case CMD_TRACK_START:
        case CMD_TRACK_STOP:
        default:
            break;
    }
}

static int send_can_frame(int canfd, uint32_t can_id, const uint8_t *data, uint8_t dlc) {
    struct can_frame f;
    memset(&f, 0, sizeof(f));
    f.can_id  = can_id;
    f.can_dlc = dlc;
    memcpy(f.data, data, dlc);

    ssize_t wn = write(canfd, &f, sizeof(f));
    if (wn != (ssize_t)sizeof(f)) {
        perror("write(can)");
        return -1;
    }
    return 0;
}

// TCP는 스트림이라 5바이트 고정 메시지를 쪼개서 받아야 함
typedef struct {
    int fd;// 통신 소켓 fd;
    uint8_t buf[sizeof(Ctrl_Message)]; //수신 버퍼
    size_t have; //버퍼에 write한 byte수
} TcpClient;

static void tcp_client_reset(TcpClient *c) {
    c->fd = -1;
    c->have = 0;
    memset(c->buf, 0, sizeof(c->buf));
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <can_ifname> <udp_drive_port> <tcp_ctrl_port> <verbose0or1>\n", argv[0]);
        fprintf(stderr, "Example: %s can0 5000 6000 1\n", argv[0]);
        return 1;
    }

    const char *can_ifname = argv[1];
    int udp_port = atoi(argv[2]);
    int tcp_port = atoi(argv[3]);
    int verbose  = atoi(argv[4]);

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    if (sizeof(Ctrl_Message) != 5) {
        fprintf(stderr, "ERROR: sizeof(Ctrl_Message)=%zu (expected 5). Check packing/headers.\n",
                sizeof(Ctrl_Message));
        return 1;
    }

    int canfd = open_can_socket(can_ifname);
    if (canfd < 0) return 1;

    int udpfd = open_udp_listener(udp_port);
    if (udpfd < 0) { close(canfd); return 1; }

    int tcp_listen = open_tcp_listener(tcp_port);
    if (tcp_listen < 0) { close(udpfd); close(canfd); return 1; }

    printf("Relay started\n");
    printf(" CAN : %s\n", can_ifname);
    printf(" UDP : 0.0.0.0:%d (DRIVE: 4 bytes steeringBE2+gear+speed)\n", udp_port);
    printf(" TCP : 0.0.0.0:%d (CTRL : fixed %zu bytes Ctrl_Message)\n", tcp_port, sizeof(Ctrl_Message));
    printf("Press Ctrl+C to stop.\n");

    // 단순하게 TCP client는 여러개 지원 (최대 8개)
    TcpClient clients[8];
    for (size_t i = 0; i < ARRAY_SIZE(clients); i++) tcp_client_reset(&clients[i]);

    while (g_running) {
        struct pollfd pfds[1 + 1 + ARRAY_SIZE(clients)];
        nfds_t nfds = 0;

        // UDP
        pfds[nfds].fd = udpfd;
        pfds[nfds].events = POLLIN;
        nfds++;

        // TCP listen
        pfds[nfds].fd = tcp_listen;
        pfds[nfds].events = POLLIN;
        nfds++;

        // TCP clients
        for (size_t i = 0; i < ARRAY_SIZE(clients); i++) {
            if (clients[i].fd >= 0) {
                pfds[nfds].fd = clients[i].fd;
                pfds[nfds].events = POLLIN;
                nfds++;
            }
        }

        int pr = poll(pfds, nfds, 200); // 200ms
        if (pr < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        if (pr == 0) continue;

        nfds_t idx = 0;

        // =========
        // UDP 처리 (DRIVE) - 드레인해서 최신값 1개만 송신
        // =========
        if (pfds[idx].revents & POLLIN) {
            uint8_t buf[64];
            struct sockaddr_in src;
            socklen_t slen = sizeof(src);

            int got_latest = 0;
            int16_t steering_last = 0;
            uint8_t gear_last = 0;
            uint8_t speed_last = 0;
            struct sockaddr_in src_last;
            memset(&src_last, 0, sizeof(src_last));

            for (;;) {
                ssize_t n = recvfrom(udpfd, buf, sizeof(buf), 0, (struct sockaddr*)&src, &slen);
                if (n < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break; // 드레인 완료
                    }
                    perror("recvfrom(udp)");
                    break;
                }

                if (n == 4) {
                    steering_last = parse_i16_be(&buf[0]);
                    gear_last = buf[2];
                    speed_last = buf[3];
                    src_last = src;
                    got_latest = 1;
                } else if (verbose) {
                    char ipbuf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &src.sin_addr, ipbuf, sizeof(ipbuf));
                    printf("[UDP] %s:%u WARN: got %zd bytes (expected 4)\n",
                           ipbuf, ntohs(src.sin_port), n);
                }
            }

            if (got_latest) {
                Ctrl_Message m;
                memset(&m, 0, sizeof(m));
                m.cmd = CMD_DRIVE;
                m.payload.drive_payload.steering_deg = steering_last;
                m.payload.drive_payload.gear = gear_last;
                m.payload.drive_payload.speed = speed_last;

                
                uint8_t can_data[5];
                encode_ctrl_to_can_data(can_data, &m, /*drive_steering_is_host_endian=*/1);

                uint32_t can_id = can_id_from_cmd(m.cmd);
                if (can_id != 0) {
                    send_can_frame(canfd, can_id, can_data, 5);
                    if (verbose) {
                        char ipbuf[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &src_last.sin_addr, ipbuf, sizeof(ipbuf));
                        printf("[UDP] %s:%u DRIVE(latest) steer=%d gear=%u speed=%u -> CAN 0x%03X\n",
                               ipbuf, ntohs(src_last.sin_port), steering_last, gear_last, speed_last, can_id);
                    }
                }
            }
        }
        idx++;

        // =========
        // TCP accept
        // =========
        if (pfds[idx].revents & POLLIN) {
            struct sockaddr_in cli;
            socklen_t clen = sizeof(cli);
            int cfd = accept(tcp_listen, (struct sockaddr*)&cli, &clen);
            if (cfd >= 0) {
                // 빈 슬롯 찾기
                size_t slot = ARRAY_SIZE(clients);
                for (size_t i = 0; i < ARRAY_SIZE(clients); i++) {
                    if (clients[i].fd < 0) { slot = i; break; }
                }
                if (slot == ARRAY_SIZE(clients)) {
                    if (verbose) printf("[TCP] Too many clients, rejecting.\n");
                    close(cfd);
                } else {
                    clients[slot].fd = cfd;
                    clients[slot].have = 0;

                    if (verbose) {
                        char ipbuf[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &cli.sin_addr, ipbuf, sizeof(ipbuf));
                        printf("[TCP] Client connected: %s:%u (slot %zu)\n",
                               ipbuf, ntohs(cli.sin_port), slot);
                    }
                }
            }
        }
        idx++;

        // =========
        // TCP clients read (fixed 5 bytes Ctrl_Message)
        // =========
        for (size_t i = 0; i < ARRAY_SIZE(clients); i++) {
            if (clients[i].fd < 0) continue;

            // pollfd는 "활성 client만" 넣었으므로, idx를 순차적으로 따라가야 함
            if (!(pfds[idx].revents & POLLIN) && !(pfds[idx].revents & (POLLHUP | POLLERR | POLLNVAL))) {
                idx++;
                continue;
            }

            if (pfds[idx].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                if (verbose) printf("[TCP] Client slot %zu disconnected (poll flags).\n", i);
                close(clients[i].fd);
                tcp_client_reset(&clients[i]);
                idx++;
                continue;
            }

            // 읽기
            ssize_t n = recv(clients[i].fd,
                             clients[i].buf + clients[i].have,
                             sizeof(Ctrl_Message) - clients[i].have,
                             0);
            if (n > 0) {
                clients[i].have += (size_t)n;

                // 5바이트가 쌓일 때마다 처리
                while (clients[i].have >= sizeof(Ctrl_Message)) {
                    Ctrl_Message m;
                    memcpy(&m, clients[i].buf, sizeof(m));

                    // 남은 데이터 당기기 (추가 메시지 대비)
                    size_t remain = clients[i].have - sizeof(Ctrl_Message);
                    if (remain > 0) memmove(clients[i].buf, clients[i].buf + sizeof(Ctrl_Message), remain);
                    clients[i].have = remain;

                    uint32_t can_id = can_id_from_cmd(m.cmd);
                    if (can_id == 0) {
                        if (verbose) printf("[TCP] slot %zu WARN: unknown cmd=0x%02X\n", i, m.cmd);
                        continue;
                    }

                    uint8_t can_data[5];
                    encode_ctrl_to_can_data(can_data, &m, /*drive_steering_is_host_endian=*/1);

                    send_can_frame(canfd, can_id, can_data, 5);

                    if (verbose) {
                        printf("[TCP] slot %zu cmd=0x%02X -> CAN 0x%03X (DLC=5)\n", i, m.cmd, can_id);
                    }
                }
            } else if (n == 0) {
                if (verbose) printf("[TCP] Client slot %zu closed.\n", i);
                close(clients[i].fd);
                tcp_client_reset(&clients[i]);
            } else {
                if (errno != EINTR) perror("recv(tcp client)");
                close(clients[i].fd);
                tcp_client_reset(&clients[i]);
            }

            idx++;
        }
    }

    printf("Shutting down...\n");
    for (size_t i = 0; i < ARRAY_SIZE(clients); i++) {
        if (clients[i].fd >= 0) close(clients[i].fd);
    }
    close(tcp_listen);
    close(udpfd);
    close(canfd);
    return 0;
}
