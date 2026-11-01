#include "stdafx.h"
#include "VoiceChat.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mmsystem.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

namespace
{
    CVoiceChat* g_voice = nullptr;
    const char kMagic[] = "MUV1";
    const int kPacketHeader = 8;
}

CVoiceChat& CVoiceChat::Instance()
{
    static CVoiceChat instance;
    return instance;
}

bool CVoiceChat::Initialize(HWND window)
{
    if (m_initialized) return true;
    m_window = window;
    g_voice = this;

    char enabled[8] = {};
    GetPrivateProfileStringA("VoiceSystem", "Enable", "0", enabled, sizeof(enabled), ".\\Data\\Custom\\VoiceSystem.txt");
    if (atoi(enabled) == 0) return false;

    char url[256] = {};
    GetPrivateProfileStringA("VoiceSystem", "VoiceServerURL", "ws://127.0.0.1:7880", url, sizeof(url), ".\\Data\\Custom\\VoiceSystem.txt");
    std::string endpoint(url);
    const std::string prefix = "ws://";
    if (endpoint.compare(0, prefix.size(), prefix) == 0) endpoint.erase(0, prefix.size());
    const size_t colon = endpoint.rfind(':');
    std::string host = colon == std::string::npos ? endpoint : endpoint.substr(0, colon);
    unsigned short port = (unsigned short)(colon == std::string::npos ? 7880 : atoi(endpoint.substr(colon + 1).c_str()));
    if (host.empty()) host = "127.0.0.1";

    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) { WSACleanup(); return false; }
    u_long nonBlocking = 1;
    ioctlsocket(m_socket, FIONBIO, &nonBlocking);

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &server.sin_addr) != 1)
    {
        hostent* resolved = gethostbyname(host.c_str());
        if (!resolved) { closesocket(m_socket); m_socket = INVALID_SOCKET; WSACleanup(); return false; }
        memcpy(&server.sin_addr, resolved->h_addr, resolved->h_length);
    }
    connect(m_socket, reinterpret_cast<sockaddr*>(&server), sizeof(server));

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = 8000;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = 16000;
    if (waveInOpen(&m_waveIn, WAVE_MAPPER, &format, (DWORD_PTR)&CaptureCallback, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR ||
        waveOutOpen(&m_waveOut, WAVE_MAPPER, &format, (DWORD_PTR)&PlaybackCallback, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
    {
        Shutdown();
        return false;
    }
    for (int i = 0; i < 2; ++i)
    {
        m_captureHeaders[i].lpData = m_captureBuffers[i];
        m_captureHeaders[i].dwBufferLength = sizeof(m_captureBuffers[i]);
        waveInPrepareHeader(m_waveIn, &m_captureHeaders[i], sizeof(WAVEHDR));
        waveInAddBuffer(m_waveIn, &m_captureHeaders[i], sizeof(WAVEHDR));
    }
    waveInStart(m_waveIn);
    m_initialized = true;
    m_receiveThread = CreateThread(nullptr, 0, [](LPVOID) -> DWORD { g_voice->ReceiveLoop(); return 0; }, nullptr, 0, nullptr);
    if (!m_receiveThread)
    {
        Shutdown();
        return false;
    }
    return true;
}

void CVoiceChat::Shutdown()
{
    m_initialized = false;
    if (m_waveIn) { waveInStop(m_waveIn); waveInReset(m_waveIn); }
    if (m_receiveThread) { WaitForSingleObject(m_receiveThread, 1000); CloseHandle(m_receiveThread); m_receiveThread = nullptr; }
    if (m_waveIn) { for (auto& h : m_captureHeaders) waveInUnprepareHeader(m_waveIn, &h, sizeof(WAVEHDR)); waveInClose(m_waveIn); m_waveIn = nullptr; }
    if (m_waveOut) { waveOutReset(m_waveOut); waveOutClose(m_waveOut); m_waveOut = nullptr; }
    if (m_socket != INVALID_SOCKET) { closesocket(m_socket); m_socket = INVALID_SOCKET; WSACleanup(); }
    g_voice = nullptr;
}

void CALLBACK CVoiceChat::CaptureCallback(HWAVEIN, UINT message, DWORD_PTR instance, DWORD_PTR param, DWORD_PTR)
{
    if (message == WIM_DATA && g_voice) g_voice->OnCapture(reinterpret_cast<WAVEHDR*>(param));
}

void CVoiceChat::OnCapture(WAVEHDR* header)
{
    if (!header || m_socket == INVALID_SOCKET) return;
    const bool pressed = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
    m_speaking = pressed;
    if (pressed)
    {
        unsigned char packet[8 + 640]{};
        memcpy(packet, kMagic, 4);
        packet[4] = 2; packet[5] = 3; packet[6] = 0; packet[7] = 0;
        send(m_socket, reinterpret_cast<const char*>(packet), kPacketHeader + header->dwBytesRecorded, 0);
    }
    waveInAddBuffer(m_waveIn, header, sizeof(WAVEHDR));
}

void CALLBACK CVoiceChat::PlaybackCallback(HWAVEOUT, UINT, DWORD_PTR, DWORD_PTR param, DWORD_PTR)
{
    if (param) delete[] reinterpret_cast<char*>(param);
}

void CVoiceChat::ReceiveLoop()
{
    char packet[65536];
    while (m_initialized && m_socket != INVALID_SOCKET)
    {
        const int received = recv(m_socket, packet, sizeof(packet), 0);
        if (received > kPacketHeader && packet[0] == 'M' && packet[1] == 'U' && packet[2] == 'V' && packet[3] == '1' && packet[4] == 2)
        {
            const int audioSize = received - kPacketHeader;
            char* block = new char[sizeof(WAVEHDR) + audioSize];
            WAVEHDR* header = reinterpret_cast<WAVEHDR*>(block);
            memset(header, 0, sizeof(WAVEHDR));
            header->lpData = block + sizeof(WAVEHDR);
            header->dwBufferLength = audioSize;
            memcpy(header->lpData, packet + kPacketHeader, audioSize);
            waveOutPrepareHeader(m_waveOut, header, sizeof(WAVEHDR));
            waveOutWrite(m_waveOut, header, sizeof(WAVEHDR));
        }
        Sleep(2);
    }
}
