#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

static int16_t parse_i16_be(const uint8_t b[2]) {
    uint16_t u = ((uint16_t)b[0] << 8) | (uint16_t)b[1];
    return (int16_t)ntohs(u); // htons/ntohs는 uint16_t지만 2's complement 그대로 유지됨
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <listen_port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);      // 0.0.0.0
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return 1;
    }

    printf("UDP receiver listening on 0.0.0.0:%d\n", port);
    printf("Expecting 4 bytes: steering(int16 BE) + gear(uint8) + speed(uint8)\n");

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
        uint8_t gear  = buf[2];
        uint8_t speed = buf[3];

        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src.sin_addr, ipbuf, sizeof(ipbuf));

        printf("from %s:%u | steering=%d deg | gear=%u | speed=%u\n",
               ipbuf, ntohs(src.sin_port), steering, gear, speed);

        // 여기서 실제 RC카 제어 함수 호출하면 됨:
        // rc_set_steering(steering);
        // rc_set_gear(gear);
        // rc_set_speed(speed);
    }

    close(fd);
    return 0;
}
