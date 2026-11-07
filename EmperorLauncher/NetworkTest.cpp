#include "NetworkTest.hpp"
#include "Lang.hpp"
#include <windows.h>
#include <winsock.h>
#include "../EmperorHooks/Error.hpp"
#include "../EmperorHooks/WolCommon.hpp"
#include "../EmperorHooks/CRC.hpp"
#include "../EmperorHooks/ResolveHost.hpp"
#pragma comment (lib, "Ws2_32.lib")

void testNetwork(HWND parent, const std::string& serverAddrString)
{
  WSAData wSAData = {};
  // init api version 1.1, because that's what emperor uses
  WSAStartup(MAKEWORD(1, 1), &wSAData);

  sockaddr_in serverAddr = {};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(wolPortH);

  // Accept an IPv4 literal or a hostname / dynamic DNS name, matching what the
  // in-game proxy does. Fall back to inet_addr for a raw IP.
  uint32_t resolvedAddr = 0;
  if (resolveHostV4(serverAddrString, resolvedAddr))
    serverAddr.sin_addr.S_un.S_addr = resolvedAddr;
  else
    serverAddr.sin_addr.S_un.S_addr = inet_addr(serverAddrString.c_str());

  bool udpOk = false;
  {
    SOCKET udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    release_assert(udpSock != INVALID_SOCKET);

    u_long mode = 1;  // 1 to enable non-blocking socket
    ioctlsocket(udpSock, FIONBIO, &mode);

    int32_t packetsPerSecond = 5;
    for (int32_t i = 0; i < packetsPerSecond * 3; i++)
    {
      ForwardedPacketHeader keepalivePacket = {};
      keepalivePacket.type = PacketType::ConnectionTest;
      keepalivePacket.crc = CRC::Calculate(((uint8_t*)&keepalivePacket) + 4, sizeof(ForwardedPacketHeader) - 4, CRC::CRC_32());
      sendto(udpSock, (const char*)&keepalivePacket, sizeof(ForwardedPacketHeader), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

      constexpr int buffSize = 5000;
      uint8_t buff[buffSize];
      int serverAddrSize = sizeof(serverAddr);
      int bytesRead = recvfrom(udpSock, (char*)&buff[0], buffSize, 0, (sockaddr*)&serverAddr, &serverAddrSize);

      if (bytesRead != SOCKET_ERROR && bytesRead == sizeof(ForwardedPacketHeader))
      {
        ForwardedPacketHeader* header = (ForwardedPacketHeader*)buff;
        uint32_t realCrc = CRC::Calculate(buff + 4, bytesRead - 4, CRC::CRC_32());
        if (realCrc == header->crc && header->type == PacketType::ConnectionTest)
        {
          udpOk = true;
          break;
        }
      }

      Sleep(1000 / packetsPerSecond);
    }
  }

  bool tcpOk = false;
  {
    SOCKET tcpSock = socket(AF_INET, SOCK_STREAM, 0);
    release_assert(tcpSock != INVALID_SOCKET);

    // set non blocking
    u_long mode = 1;
    ioctlsocket(tcpSock, FIONBIO, &mode);

    bool connected = 0;
    {
      int result = connect(tcpSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
      if (result == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
      {
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(tcpSock, &writefds);

        timeval timeout = {};
        timeout.tv_sec = 3;

        result = select(0, NULL, &writefds, NULL, &timeout);
        if (result != 0 && FD_ISSET(tcpSock, &writefds))
        {
          int so_error;
          int len = sizeof(so_error);
          getsockopt(tcpSock, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
          if (so_error == 0)
            connected = true;
        }
      }
    }

    // set blocking again
    mode = 0;
    ioctlsocket(tcpSock, FIONBIO, &mode);

    DWORD timeoutMs = 1000 * 3;
    setsockopt(tcpSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(DWORD));
    setsockopt(tcpSock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutMs, sizeof(DWORD));

    std::string message = "QUIT\r\n";
    if (send(tcpSock, message.c_str(), message.size(), 0) == int32_t(message.size()))
    {
      constexpr int buffSize = 5000;
      char buff[buffSize];
      buff[0] = 0;

      int ret = recv(tcpSock, buff, buffSize, 0);

      if (ret > 0 && ret < buffSize)
      {
        buff[ret] = 0;

        if (buff == std::string_view(": 607\r\n"))
          tcpOk = true;
      }
    }

    closesocket(tcpSock);
  }

  WSACleanup();

  if (!udpOk || !tcpOk)
  {
    std::wstring message = std::wstring(L"UDP port 4005: ") + (udpOk ? L"OK" : T(L"FAIL", L"FEHLER")) + L"\n";
    message += std::wstring(L"TCP port 4005: ") + (tcpOk ? L"OK" : T(L"FAIL", L"FEHLER")) + L"\n";
    message += T(L"\nMake sure:\n- host has the game running (clicked play, not just opened this launcher)\n- host has opened port ",
                 L"\nBitte pr\u00fcfen:\n- Beim Host l\u00e4uft das Spiel (auf Spielen geklickt, nicht nur den Launcher ge\u00f6ffnet)\n- Der Host hat Port ");
    message += std::to_wstring(wolPortH);
    message += T(L" open on both UDP and TCP", L" f\u00fcr UDP und TCP freigegeben");

    MessageBoxW(parent, message.c_str(), T(L"Failure", L"Fehlgeschlagen"), 0);
    return;
  }

  MessageBoxW(parent, T(L"Connection succeeded", L"Verbindung erfolgreich"), T(L"Success", L"Erfolg"), 0);
}