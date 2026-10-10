// Independent boot probe: record the first complete HTTP 200 response on nginx.
// Build with the board toolchain; it uses no management-runtime libraries.
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

static double uptime() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const double started = uptime();
    unsigned attempts = 0;
    while (uptime() - started < 120) {
        ++attempts;
        const int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) return 3;
        timeval timeout{0, 50000};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(80);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        std::string response;
        bool complete = false;
        if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
            const char request[] = "GET /api/instruments/details HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n";
            if (send(fd, request, sizeof(request) - 1, MSG_NOSIGNAL) == sizeof(request) - 1) {
                char buffer[8192];
                ssize_t count;
                while ((count = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
                    response.append(buffer, count);
                    if (response.size() > 1024 * 1024) break;
                }
                complete = count == 0;
            }
        }
        close(fd);
        if (complete && (response.starts_with("HTTP/1.1 200 ") || response.starts_with("HTTP/1.0 200 ")) &&
            response.find("\r\n\r\n") != std::string::npos && response.find("\"instruments\"") != std::string::npos) {
            FILE* output = fopen(argv[1], "w");
            if (!output) return 4;
            fprintf(output, "{\"probe_started_s\":%.9f,\"nginx_api_ready_s\":%.9f,\"attempts\":%u}\n", started, uptime(), attempts);
            return fclose(output) == 0 ? 0 : 5;
        }
        usleep(10000);
    }
    return 1;
}
