#include "protocol.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

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

socket_t connect_to(const std::string& host, uint16_t port) {
    socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == invalid_socket_value) {
        return invalid_socket_value;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close_socket(fd);
        return invalid_socket_value;
    }
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket(fd);
        return invalid_socket_value;
    }
    return fd;
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

bool send_line(socket_t fd, const std::string& line) {
    const std::vector<uint8_t> bytes(line.begin(), line.end());
    return send_all(fd, bytes);
}

void set_recv_timeout(socket_t fd, int milliseconds) {
#ifdef _WIN32
    DWORD timeout = static_cast<DWORD>(milliseconds);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval timeout{};
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
}

struct GatewayStats {
    uint64_t total = 0;
    uint64_t success = 0;
    uint64_t failed = 0;
    uint64_t timeout = 0;
    uint64_t total_latency_ms = 0;

    std::string report() const {
        std::ostringstream oss;
        const double avg = success == 0 ? 0.0 : static_cast<double>(total_latency_ms) / success;
        oss << "stats total=" << total
            << " success=" << success
            << " failed=" << failed
            << " timeout=" << timeout
            << " avg_ms=" << std::fixed << std::setprecision(2) << avg;
        return oss.str();
    }
};

bool recv_line(socket_t fd, std::string& line) {
    line.clear();
    char ch = 0;
    while (true) {
        const int n = recv(fd, &ch, 1, 0);
        if (n <= 0) {
            return false;
        }
        if (ch == '\n') {
            return true;
        }
        if (ch != '\r') {
            line.push_back(ch);
        }
    }
}

bool parse_command(const std::string& line, uint8_t seq, proto::Frame& frame) {
    std::istringstream iss(line);
    std::string op;
    iss >> op;
    frame.seq = seq;

    if (op == "status") {
        frame.cmd = proto::Command::GetStatus;
        frame.payload.clear();
        return true;
    }
    if (op == "sensor") {
        frame.cmd = proto::Command::GetSensor;
        frame.payload.clear();
        return true;
    }
    if (op == "led") {
        std::string value;
        iss >> value;
        frame.cmd = proto::Command::SetLed;
        frame.payload = proto::text_payload(value);
        return !value.empty();
    }
    if (op == "pwm") {
        std::string value;
        iss >> value;
        int pwm = -1;
        try {
            pwm = std::stoi(value);
        } catch (...) {
            pwm = -1;
        }
        if (pwm < 0 || pwm > 100) {
            return false;
        }
        frame.cmd = proto::Command::SetPwm;
        frame.payload = proto::u8_payload(static_cast<uint8_t>(pwm));
        return true;
    }
    if (op == "reset_error") {
        frame.cmd = proto::Command::ResetError;
        frame.payload.clear();
        return true;
    }
    if (op == "version") {
        frame.cmd = proto::Command::GetVersion;
        frame.payload.clear();
        return true;
    }
    return false;
}

bool read_device_response(socket_t device, proto::Frame& response) {
    RingBuffer<512> rx;
    uint8_t buf[128]{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);

    while (std::chrono::steady_clock::now() < deadline) {
        const int n = recv(device, reinterpret_cast<char*>(buf), sizeof(buf), 0);
        if (n <= 0) {
            return false;
        }
        for (int i = 0; i < n; ++i) {
            rx.push(buf[i]);
        }
        if (auto frame = proto::try_decode(rx)) {
            response = *frame;
            return true;
        }
    }
    return false;
}

std::string commands_help() {
    return "commands: status | sensor | version | led on/off | pwm 0..100 | reset_error | stats | quit\n";
}

} // namespace

int main(int argc, char** argv) {
    const uint16_t pc_port = argc > 1 ? static_cast<uint16_t>(std::stoi(argv[1])) : 9000;
    const uint16_t device_port = argc > 2 ? static_cast<uint16_t>(std::stoi(argv[2])) : 9100;

    if (!init_sockets()) {
        std::cerr << "socket init failed\n";
        return 1;
    }

    socket_t device = invalid_socket_value;
    for (int attempt = 1; attempt <= 20; ++attempt) {
        device = connect_to("127.0.0.1", device_port);
        if (device != invalid_socket_value) {
            break;
        }
        std::cout << "[gateway] waiting for device simulator, attempt " << attempt << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    if (device == invalid_socket_value) {
        std::cerr << "cannot connect to device simulator on 127.0.0.1:" << device_port << "\n";
        cleanup_sockets();
        return 1;
    }
    set_recv_timeout(device, 3000);

    socket_t server = listen_on(pc_port);
    if (server == invalid_socket_value) {
        std::cerr << "gateway listen failed on port " << pc_port << "\n";
        close_socket(device);
        cleanup_sockets();
        return 1;
    }

    std::cout << "[gateway] device connected. PC client port: " << pc_port << "\n";
    std::cout << "[gateway] " << commands_help();

    socket_t pc = accept(server, nullptr, nullptr);
    if (pc == invalid_socket_value) {
        std::cerr << "pc accept failed\n";
        close_socket(server);
        close_socket(device);
        cleanup_sockets();
        return 1;
    }
    send_line(pc, "gateway ready. type help for commands.\n");

    uint8_t seq = 1;
    GatewayStats stats;
    std::string line;
    while (recv_line(pc, line)) {
        if (line == "quit" || line == "exit") {
            send_line(pc, "bye\n");
            break;
        }
        if (line == "help") {
            send_line(pc, commands_help());
            continue;
        }
        if (line == "stats") {
            send_line(pc, stats.report() + "\n");
            continue;
        }

        proto::Frame request;
        if (!parse_command(line, seq++, request)) {
            stats.total++;
            stats.failed++;
            send_line(pc, "bad command. type help.\n");
            continue;
        }

        std::cout << "[gateway] pc -> device: " << line << "\n";
        stats.total++;
        const auto begin = std::chrono::steady_clock::now();
        if (!send_all(device, proto::encode(request))) {
            stats.failed++;
            send_line(pc, "device link failed while sending\n");
            break;
        }

        proto::Frame response;
        if (!read_device_response(device, response)) {
            stats.failed++;
            stats.timeout++;
            send_line(pc, "device timeout or disconnect\n");
            break;
        }

        const auto end = std::chrono::steady_clock::now();
        const auto cost_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
        stats.total_latency_ms += static_cast<uint64_t>(cost_ms);
        if (response.cmd == proto::Command::Ack) {
            stats.success++;
        } else {
            stats.failed++;
        }

        const std::string reply = proto::command_name(response.cmd) + " seq=" +
                                  std::to_string(response.seq) + " " +
                                  proto::payload_text(response.payload) + "\n";
        send_line(pc, reply);
        std::cout << "[gateway] device -> pc: " << reply;
        std::cout << "[gateway] seq=" << static_cast<int>(response.seq)
                  << " cmd=" << proto::command_name(request.cmd)
                  << " cost_ms=" << cost_ms
                  << " " << stats.report() << "\n";
    }

    close_socket(pc);
    close_socket(server);
    close_socket(device);
    cleanup_sockets();
    return 0;
}
