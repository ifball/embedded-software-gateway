#include "protocol.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
constexpr socket_t invalid_socket_value = INVALID_SOCKET;
static void close_socket(socket_t s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
constexpr socket_t invalid_socket_value = -1;
static void close_socket(socket_t s) { close(s); }
#endif

namespace {

bool init_sockets() {
#ifdef _WIN32
    WSADATA wsa{};
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

void cleanup_sockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

socket_t listen_on(uint16_t port) {
    socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == invalid_socket_value) {
        return invalid_socket_value;
    }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket(fd);
        return invalid_socket_value;
    }
    if (listen(fd, 1) != 0) {
        close_socket(fd);
        return invalid_socket_value;
    }
    return fd;
}

bool send_all(socket_t fd, const std::vector<uint8_t>& bytes) {
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const int n = send(fd,
                           reinterpret_cast<const char*>(bytes.data() + sent),
                           static_cast<int>(bytes.size() - sent),
                           0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

std::vector<uint8_t> make_ack_payload(bool led, int pwm, int sensor, const std::string& note) {
    const std::string text = "led=" + std::string(led ? "on" : "off") +
                             ";pwm=" + std::to_string(pwm) +
                             ";sensor=" + std::to_string(sensor) +
                             ";note=" + note;
    return proto::text_payload(text);
}

} // namespace

int main(int argc, char** argv) {
    const uint16_t port = argc > 1 ? static_cast<uint16_t>(std::stoi(argv[1])) : 9100;
    if (!init_sockets()) {
        std::cerr << "socket init failed\n";
        return 1;
    }

    socket_t server = listen_on(port);
    if (server == invalid_socket_value) {
        std::cerr << "device_sim listen failed on port " << port << "\n";
        cleanup_sockets();
        return 1;
    }

    std::cout << "[device] STM32/FreeRTOS simulator listening on " << port << "\n";
    socket_t client = accept(server, nullptr, nullptr);
    if (client == invalid_socket_value) {
        std::cerr << "accept failed\n";
        close_socket(server);
        cleanup_sockets();
        return 1;
    }
    std::cout << "[device] gateway connected\n";

    RingBuffer<512> rx;
    bool led = false;
    int pwm = 0;
    int error_count = 0;
    int sensor = 250;
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> jitter(-3, 4);

    uint8_t buf[128]{};
    while (true) {
        const int n = recv(client, reinterpret_cast<char*>(buf), sizeof(buf), 0);
        if (n <= 0) {
            std::cout << "[device] gateway disconnected\n";
            break;
        }

        for (int i = 0; i < n; ++i) {
            rx.push(buf[i]);
        }

        while (auto frame = proto::try_decode(rx)) {
            sensor += jitter(rng);
            proto::Frame response;
            response.seq = frame->seq;
            response.cmd = proto::Command::Ack;

            switch (frame->cmd) {
                case proto::Command::GetStatus:
                    response.payload = make_ack_payload(led, pwm, sensor, "status_ok");
                    break;
                case proto::Command::GetSensor:
                    response.payload = proto::text_payload("sensor=" + std::to_string(sensor));
                    break;
                case proto::Command::SetLed: {
                    const std::string value = proto::payload_text(frame->payload);
                    if (value == "on" || value == "1") {
                        led = true;
                        response.payload = make_ack_payload(led, pwm, sensor, "led_updated");
                    } else if (value == "off" || value == "0") {
                        led = false;
                        response.payload = make_ack_payload(led, pwm, sensor, "led_updated");
                    } else {
                        error_count++;
                        response.cmd = proto::Command::Error;
                        response.payload = proto::text_payload("invalid led value");
                    }
                    break;
                }
                case proto::Command::SetPwm: {
                    uint8_t next_pwm = 0;
                    if (proto::payload_u8(frame->payload, next_pwm) && next_pwm <= 100) {
                        pwm = next_pwm;
                        response.payload = make_ack_payload(led, pwm, sensor, "pwm_updated");
                    } else {
                        error_count++;
                        response.cmd = proto::Command::Error;
                        response.payload = proto::text_payload("pwm payload must be one byte 0..100");
                    }
                    break;
                }
                case proto::Command::ResetError:
                    error_count = 0;
                    response.payload = proto::text_payload("error_count=0");
                    break;
                case proto::Command::GetVersion:
                    response.payload = proto::text_payload("version=1.0.0;build=release");
                    break;
                default:
                    error_count++;
                    response.cmd = proto::Command::Error;
                    response.payload = proto::text_payload("unsupported command");
                    break;
            }

            std::cout << "[device] seq=" << static_cast<int>(frame->seq)
                      << " cmd=" << proto::command_name(frame->cmd)
                      << " errors=" << error_count << "\n";
            send_all(client, proto::encode(response));
        }
    }

    close_socket(client);
    close_socket(server);
    cleanup_sockets();
    return 0;
}
