#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <cstring>
#pragma comment(lib, "ws2_32.lib")

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

#define NET_PORT        7777
#define NET_MAGIC       0x42424B32u
#define NET_MAX_CLONES  5

#pragma pack(push, 1)
struct NetClone { float x, y, angle; };
struct NetPacket {
	uint32_t magic;
	float x, y, angle;
	uint8_t swinging;
	float anchorX, anchorY;
	uint8_t cloneCount;
	NetClone clones[NET_MAX_CLONES];
};
#pragma pack(pop)

class AstroNet {
	SOCKET sock = INVALID_SOCKET;
	sockaddr_in peer{};
	bool hasPeer = false;
	bool isHost = false;

	bool openSocket(unsigned short bindPort) {
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (sock == INVALID_SOCKET) return false;

		u_long nb = 1;
		ioctlsocket(sock, FIONBIO, &nb);

		BOOL off = FALSE; DWORD ret = 0;
		WSAIoctl(sock, SIO_UDP_CONNRESET, &off, sizeof(off), NULL, 0, &ret, NULL, NULL);

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = htons(bindPort);
		return bind(sock, (sockaddr*)&addr, sizeof(addr)) == 0;
	}
public:
	bool startHost() {
		isHost = true;
		return openSocket(NET_PORT);
	}
	bool startClient(const char* ip) {
		isHost = false;
		if (!openSocket(0)) return false;
		peer.sin_family = AF_INET;
		peer.sin_port = htons(NET_PORT);
		if (inet_pton(AF_INET, ip, &peer.sin_addr) != 1) return false;
		hasPeer = true;
		return true;
	}
	void send(NetPacket p) {
		if (sock == INVALID_SOCKET || !hasPeer) return;
		p.magic = NET_MAGIC;
		sendto(sock, (const char*)&p, sizeof(p), 0, (sockaddr*)&peer, sizeof(peer));
	}
	bool receive(NetPacket& out) {
		if (sock == INVALID_SOCKET) return false;
		bool got = false;
		for (;;) {
			NetPacket p; sockaddr_in from{}; int fl = sizeof(from);
			int n = recvfrom(sock, (char*)&p, sizeof(p), 0, (sockaddr*)&from, &fl);
			if (n != (int)sizeof(p)) break;
			if (p.magic != NET_MAGIC) continue;
			if (p.cloneCount > NET_MAX_CLONES) continue;
			if (isHost && !hasPeer) { peer = from; hasPeer = true; }
			out = p; got = true;
		}
		return got;
	}
	void close() {
		if (sock != INVALID_SOCKET) { closesocket(sock); sock = INVALID_SOCKET; }
		WSACleanup();
	}
};