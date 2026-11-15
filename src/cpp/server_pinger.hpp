#pragma once
#define UNICODE
#define _UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>

struct ServerStatus {
    bool online = false;
    int latencyMs = 0;
    int playersOnline = 0;
    int playersMax = 0;
    std::wstring motd = L"";
    std::wstring version = L"";
};

class NativeServerPinger {
public:
    static ServerStatus Ping(const std::string& host, int port = 25565) {
        ServerStatus status;
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return status;

        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            return status;
        }

        DWORD timeout = 4000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

        struct addrinfo hints = { 0 }, *res = NULL;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char portStr[16];
        snprintf(portStr, sizeof(portStr), "%d", port);

        DWORD startTime = GetTickCount();

        if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0) {
            closesocket(sock);
            WSACleanup();
            return status;
        }

        if (connect(sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
            freeaddrinfo(res);
            closesocket(sock);
            WSACleanup();
            return status;
        }
        freeaddrinfo(res);

        status.latencyMs = (int)(GetTickCount() - startTime);

        // Handshake packet
        std::vector<BYTE> packet;
        packet.push_back(0x00); // Packet ID
        // Protocol version 47 (1.8 - works for SLP on all modern servers)
        packet.push_back(0x2f);
        // Host length and host
        packet.push_back((BYTE)host.length());
        for (char c : host) packet.push_back((BYTE)c);
        // Port (short big endian)
        packet.push_back((BYTE)((port >> 8) & 0xff));
        packet.push_back((BYTE)(port & 0xff));
        // Next state: 1 (status)
        packet.push_back(0x01);

        // Prepend length
        std::vector<BYTE> sendBuf;
        sendBuf.push_back((BYTE)packet.size());
        sendBuf.insert(sendBuf.end(), packet.begin(), packet.end());

        // Status request packet: length 1, packet ID 0
        sendBuf.push_back(0x01);
        sendBuf.push_back(0x00);

        send(sock, (const char*)sendBuf.data(), (int)sendBuf.size(), 0);

        char recvBuf[4096];
        int bytes = recv(sock, recvBuf, sizeof(recvBuf) - 1, 0);
        if (bytes > 0) {
            recvBuf[bytes] = 0;
            status.online = true;

            // Simple search for "description" / "text"
            char* textKey = strstr(recvBuf, "\"text\":\"");
            if (textKey) {
                textKey += 8;
                char* endQuote = strchr(textKey, '\"');
                if (endQuote) {
                    std::string motdStr(textKey, endQuote - textKey);
                    status.motd = std::wstring(motdStr.begin(), motdStr.end());
                }
            } else {
                status.motd = L"Minecraft Server";
            }

            char* onlineKey = strstr(recvBuf, "\"online\":");
            if (onlineKey) {
                sscanf(onlineKey + 9, "%d", &status.playersOnline);
            }
            char* maxKey = strstr(recvBuf, "\"max\":");
            if (maxKey) {
                sscanf(maxKey + 6, "%d", &status.playersMax);
            }
        }

        closesocket(sock);
        WSACleanup();
        return status;
    }
};
