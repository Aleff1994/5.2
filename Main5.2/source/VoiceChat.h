#pragma once

#include <winsock2.h>
#include <windows.h>

class CVoiceChat
{
public:
    static CVoiceChat& Instance();
    bool Initialize(HWND window);
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }
    bool IsSpeaking() const { return m_speaking; }

private:
    CVoiceChat() = default;
    ~CVoiceChat() = default;
    CVoiceChat(const CVoiceChat&) = delete;
    CVoiceChat& operator=(const CVoiceChat&) = delete;
    void ReceiveLoop();
    static void CALLBACK CaptureCallback(HWAVEIN, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
    static void CALLBACK PlaybackCallback(HWAVEOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
    void OnCapture(WAVEHDR* header);

    HWND m_window = nullptr;
    SOCKET m_socket = INVALID_SOCKET;
    HANDLE m_receiveThread = nullptr;
    HWAVEIN m_waveIn = nullptr;
    HWAVEOUT m_waveOut = nullptr;
    WAVEHDR m_captureHeaders[2]{};
    char m_captureBuffers[2][640]{};
    bool m_initialized = false;
    volatile bool m_speaking = false;
};

#define gVoiceChat (CVoiceChat::Instance())
