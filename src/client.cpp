#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

struct ServerInfo {
    string room_name;
    string ip;
    int tcp_port;
};

mutex server_list_mutex;
vector<ServerInfo> server_list;

SOCKET chat_socket = INVALID_SOCKET;

void udp_listener(int udp_port) {
    SOCKET udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock == INVALID_SOCKET) {
        cerr << "UDP soket oluşturulamadı\n";
        return;
    }

    sockaddr_in listen_addr{};
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(udp_port);
    listen_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_sock, (sockaddr*)&listen_addr, sizeof(listen_addr)) == SOCKET_ERROR) {
        cerr << "UDP bind başarısız\n";
        closesocket(udp_sock);
        return;
    }

    char buffer[256];
    while (true) {
        sockaddr_in sender_addr{};
        int sender_len = sizeof(sender_addr);
        int bytes = recvfrom(udp_sock, buffer, sizeof(buffer) - 1, 0,
                             (sockaddr*)&sender_addr, &sender_len);
        if (bytes <= 0) continue;

        buffer[bytes] = '\0';

        string msg(buffer);
        auto sep = msg.find(';');
        if (sep == string::npos) continue;

        ServerInfo si;
        si.room_name = msg.substr(0, sep);
        si.tcp_port = stoi(msg.substr(sep + 1));
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(sender_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
        si.ip = ip_str;

        lock_guard<mutex> lock(server_list_mutex);
        bool exists = false;
        for (auto& s : server_list) {
            if (s.ip == si.ip && s.tcp_port == si.tcp_port) {
                exists = true;
                break;
            }
        }
        if (!exists)
            server_list.push_back(si);
    }
}

void chat_receive_thread() {
    char buffer[1024];
    while (true) {
        int bytes = recv(chat_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        cout << buffer << "\n";
    }
    cout << "Sunucu bağlantısı kesildi.\n";
    closesocket(chat_socket);
    chat_socket = INVALID_SOCKET;
}

void chat_send_loop() {
    while (true) {
        string msg;
        getline(cin, msg);
        if (msg.empty()) continue;
        send(chat_socket, msg.c_str(), (int)msg.size(), 0);
    }
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "Winsock başlatılamadı\n";
        return 1;
    }

    const int udp_port = 8888;
    thread listener(udp_listener, udp_port);
    listener.detach();

    cout << "Sunucular aranıyor, lütfen bekleyin...\n";
    this_thread::sleep_for(chrono::seconds(3));

    {
        lock_guard<mutex> lock(server_list_mutex);
        if (server_list.empty()) {
            cout << "Hiç sunucu bulunamadı.\n";
            WSACleanup();
            return 1;
        }

        cout << "Sunucular:\n";
        for (size_t i = 0; i < server_list.size(); i++) {
            cout << i + 1 << ") " << server_list[i].room_name << " - " << server_list[i].ip << ":" << server_list[i].tcp_port << "\n";
        }
    }

    int choice = 0;
    cout << "Bağlanmak istediğiniz sunucu numarasını girin: ";
    cin >> choice;
    cin.ignore();

    ServerInfo selected;
    {
        lock_guard<mutex> lock(server_list_mutex);
        if (choice < 1 || choice >(int)server_list.size()) {
            cout << "Geçersiz seçim\n";
            WSACleanup();
            return 1;
        }
        selected = server_list[choice - 1];
    }

    chat_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (chat_socket == INVALID_SOCKET) {
        cerr << "TCP soket oluşturulamadı\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(selected.tcp_port);
    inet_pton(AF_INET, selected.ip.c_str(), &server_addr.sin_addr);

    if (connect(chat_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "Sunucuya bağlanılamadı\n";
        closesocket(chat_socket);
        WSACleanup();
        return 1;
    }

    cout << "Sunucuya bağlanıldı. Sohbete başlayabilirsiniz.\n";

    thread recv_thread(chat_receive_thread);
    thread send_thread(chat_send_loop);

    recv_thread.join();
    send_thread.join();

    WSACleanup();
    return 0;
}
