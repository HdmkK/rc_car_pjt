// udp_ctrl_receiver.cpp
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

struct CtrlMessage {
    uint8_t  cmd{};
    int16_t  steering_deg{};
    uint8_t  gear{};   // 0=FWD, 1=BWD
    uint8_t  speed{};  // 0~255
};

static CtrlMessage parse_ctrl_packet(const uint8_t* buf, size_t len) {
    if (len < 5) throw std::runtime_error("packet too short");

    CtrlMessage m{};
    m.cmd = buf[0];

    uint16_t steer_be{};
    std::memcpy(&steer_be, buf + 1, sizeof(steer_be));
    uint16_t steer_u = ntohs(steer_be);
    m.steering_deg = static_cast<int16_t>(steer_u);

    m.gear  = buf[3];
    m.speed = buf[4];
    return m;
}

int main(int argc, char** argv) {
    const uint16_t port = (argc >= 2) ? static_cast<uint16_t>(std::stoi(argv[1])) : 9000;

    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port   = htons(port);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        perror("bind");
        ::close(fd);
        return 1;
    }

    std::cout << "UDP listening on 0.0.0.0:" << port << "\n";

    while (true) {
        uint8_t buf[2048];
        sockaddr_in src{};
        socklen_t slen = sizeof(src);

        ssize_t n = ::recvfrom(fd, buf, sizeof(buf), 0,
                               reinterpret_cast<sockaddr*>(&src), &slen);
        if (n < 0) {
            perror("recvfrom");
            continue;
        }

        char src_ip[INET_ADDRSTRLEN]{};
        ::inet_ntop(AF_INET, &src.sin_addr, src_ip, sizeof(src_ip));
        uint16_t src_port = ntohs(src.sin_port);

        if (n < 5) {
            std::cerr << "drop: too short (" << n << "B) from "
                      << src_ip << ":" << src_port << "\n";
            continue;
        }

        try {
            CtrlMessage msg = parse_ctrl_packet(buf, static_cast<size_t>(n));

            std::string gear_str =
                (msg.gear == 0) ? "FWD" :
                (msg.gear == 1) ? "BWD" :
                ("UNKNOWN(" + std::to_string(msg.gear) + ")");

            std::cout << "from " << src_ip << ":" << src_port
                      << " | cmd=0x" << std::hex << (int)msg.cmd << std::dec
                      << " steering=" << msg.steering_deg
                      << " gear=" << gear_str
                      << " speed=" << (int)msg.speed
                      << "\n";
        } catch (const std::exception& e) {
            std::cerr << "parse error from " << src_ip << ":" << src_port
                      << " : " << e.what() << "\n";
        }
    }

    ::close(fd);
    return 0;
}

