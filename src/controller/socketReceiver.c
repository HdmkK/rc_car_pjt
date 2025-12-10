#define _POSIX_C_SOURCE 200809L
#include "ctrl_protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

static void hexdump(const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char*)buf;
    for (size_t i = 0; i < len; i++) {
        printf("%02X", p[i]);
        if (i + 1 != len) printf(" ");
    }
    printf("\n");
}

static int recv_all(int fd, void *buf, size_t len) {
    unsigned char *p = (unsigned char*)buf;
    size_t recvd = 0;

    while (recvd < len) {
        ssize_t n = recv(fd, p + recvd, len - recvd, 0);
        if (n > 0) { recvd += (size_t)n; continue; }
        if (n == 0) return 0;          // peer closed
        if (errno == EINTR) continue;  // retry
        return -1;                     // error
    }
    return 1; // success
}

static void print_message(const Ctrl_Message *m) {
    printf("=== Ctrl_Message ===\n");
    printf("cmd: 0x%02X\n", m->cmd);

    switch (m->cmd) {
        case CMD_TRACK_START:
            printf("payload: TRACK_START\n");
            break;

        case CMD_TRACK_STOP:
            printf("payload: TRACK_STOP\n");
            break;

        case CMD_HEADLIGHT: {
            const HeadLight_Ctrl_Payload *p = &m->payload.headlight_ctrl_payload;
            printf("payload: HEADLIGHT r=%u g=%u b=%u brightness=%u\n",
                   p->r, p->g, p->b, p->brightness);
            break;
        }

        case CMD_LASER: {
            const Laser_Ctrl_Payload *p = &m->payload.laser_ctrl_payload;
            printf("payload: LASER on=%u\n", p->on);
            break;
        }

        // case CMD_DRIVE: {
        //     const Drive_Payload *p = &m->payload.drive_payload;
        //     printf("payload: DRIVE speed=%d steer=%d\n", p->speed, p->steer);
        //     break;
        // }

        default:
            printf("payload: UNKNOWN (raw bytes of entire message):\n");
            hexdump(m, sizeof(*m));
            break;
    }

    printf("\n");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    int one = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
    addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 16) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("Listening on 0.0.0.0:%d\n", port);
    printf("Expecting fixed-size packed messages: %zu bytes\n", sizeof(Ctrl_Message));

    while (1) {
        struct sockaddr_in cli;
        socklen_t cli_len = sizeof(cli);
        int cfd = accept(listen_fd, (struct sockaddr*)&cli, &cli_len);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, ipbuf, sizeof(ipbuf));
        printf("Client connected: %s:%u\n", ipbuf, ntohs(cli.sin_port));

        while (1) {
            Ctrl_Message m;
            int r = recv_all(cfd, &m, sizeof(m));
            if (r == 1) {
                print_message(&m);
            } else if (r == 0) {
                printf("Client disconnected.\n");
                break;
            } else {
                perror("recv");
                break;
            }
        }

        close(cfd);
    }

    close(listen_fd);
    return 0;
}
