// driveRecvAndCanTx.c
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

// CAN 관련 헤더 (SocketCAN)
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>

// UDP로 받은 big-endian int16을 host-endian int16으로 변환
static int16_t parse_i16_be(const uint8_t b[2]) {
    uint16_t u = ntohs(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
    return (int16_t)u;  // big-endian을 그대로 16비트로 조합하면, host에서 int16으로 해석 가능
}

// SocketCAN용 CAN 소켓 열기
static int open_can_socket(const char *ifname) {
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("socket(PF_CAN)");
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    // 인터페이스 이름 설정 (예: "can0")
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

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <listen_port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    // 1) UDP 소켓 생성
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);      // 0.0.0.0
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return 1;
    }

    printf("UDP receiver listening on 0.0.0.0:%d\n", port);
    printf("Expecting 4 bytes: steering(int16 BE) + gear(uint8) + speed(uint8)\n");

    // 2) CAN 소켓 생성 (예: can0)
    const char *can_ifname = "can0";   // 필요하면 "can1" 등으로 수정
    int canfd = open_can_socket(can_ifname);
    if (canfd < 0) {
        fprintf(stderr, "Failed to open CAN interface %s\n", can_ifname);
        close(fd);
        return 1;
    }

    printf("CAN sender using interface %s\n", can_ifname);
    printf("Forwarding received UDP payload (4 bytes) as CAN frame data (ID=0x123)\n");

    while (1) {
        uint8_t buf[4];
        struct sockaddr_in src;
        socklen_t slen = sizeof(src);

        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&src, &slen);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recvfrom");
            break;
        }
        if (n != 4) {
            char ipbuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &src.sin_addr, ipbuf, sizeof(ipbuf));
            printf("[WARN] got %zd bytes from %s:%u (expected 4)\n",
                   n, ipbuf, ntohs(src.sin_port));
            continue;
        }

        int16_t steering = parse_i16_be(&buf[0]);
        uint8_t gear     = buf[2];
        uint8_t speed    = buf[3];

        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src.sin_addr, ipbuf, sizeof(ipbuf));

        printf("from %s:%u | steering=%d deg | gear=%u | speed=%u\n",
               ipbuf, ntohs(src.sin_port), steering, gear, speed);

        // ==============================
        // 여기서 CAN 프레임으로 그대로 송신
        // ==============================
        struct can_frame frame;
        memset(&frame, 0, sizeof(frame));

        frame.can_id  = 0x123;   // ★ 이 ID로 라즈3에서 필터 걸어서 받으면 됨
        frame.can_dlc = 4;       // 데이터 길이 4바이트

        // UDP에서 받은 4바이트를 그대로 CAN data로 복사
        memcpy(frame.data, buf, 4);

        ssize_t wn = write(canfd, &frame, sizeof(frame));
        if (wn != (ssize_t)sizeof(frame)) {
            perror("write(can)");
            // 계속 돌리려면 break 대신 continue도 가능
        }
    }

    close(canfd);
    close(fd);
    return 0;
}
