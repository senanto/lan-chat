#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

std::vector<SOCKET> clients;
std::mutex clients_mutex;

const std::string room_name = "github.com/senanto/lan-chat";
const int tcp_port = 5555;
const int udp_port = 8888;

void udp_broadcast() {
    SOCKET udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock == INVALID_SOCKET) {
        std::cerr << "UDP socket oluşturulamadı.\n";
        return;
    }

    char opt = 1;
    setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

    sockaddr_in broadcastAddr{};
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(udp_port);
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

    std::string message = room_name + ";" + std::to_string(tcp_port);

    while (true) {
        sendto(udp_sock, message.c_str(), (int)message.size(), 0,
               (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void handle_client(SOCKET client) {
    char buffer[1024];
    while (true) {
        int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;

        buffer[bytes] = '\0';

        std::lock_guard<std::mutex> lock(clients_mutex);
        for (SOCKET other : clients) {
            if (other != client) {
                send(other, buffer, bytes, 0);
            }
        }
    }

    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
    closesocket(client);
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Winsock başlatılamadı.\n";
        return 1;
    }

    std::thread(udp_broadcast).detach();

    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        std::cerr << "TCP socket oluşturulamadı.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(tcp_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Bind başarısız.\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen başarısız.\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    std::cout << "Sunucu başlatıldı. Oda: " << room_name << " Port: " << tcp_port << "\n";

    while (true) {
        SOCKET client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) {
            std::cerr << "Accept başarısız.\n";
            continue;
        }

        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back(client);
        std::thread(handle_client, client).detach();
    }

    WSACleanup();
    return 0;
}
