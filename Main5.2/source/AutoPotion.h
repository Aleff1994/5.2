#pragma once
#include <windows.h>
class CAutoPotion
{
public:
    CAutoPotion();
    virtual ~CAutoPotion();
    void CreateAutoPotion();
    void CheckAutoPotion();

    // NOVO: chamado pela THREAD PRINCIPAL (dentro de winLoop, perto do ProtocolCompiler).
    // E' aqui, e so' aqui, que o pacote de uso de item e' de fato enviado.
    void ProcessMainThread();

    static DWORD WINAPI AutoPotionWorker(CAutoPotion* AutoPotion);
private:
    bool m_Potion;            // Flag
    HANDLE m_Thread;
    DWORD m_LastUseTick;      // ultimo tick (GetTickCount) em que o uso foi processado, usado pro cooldown

    // NOVO: setada pela thread do AutoPotion, lida/limpa pela thread principal.
    // bool simples nao precisa de lock aqui: na pior hipotese processa um ciclo
    // (10ms) depois, sem risco nenhum de corromper o socket.
    volatile bool m_WantsUsePotion;

    // NOVO: qual item (1=Q, 2=W, 3=E, 4=R) tenta na proxima vez. O jogo so
    // permite UM uso de item por vez (trava global EnableUse, ver
    // ProtocolSend.h), entao se sempre tentassemos Q primeiro, ele sempre
    // "roubaria a vez" e W/E/R nunca seriam usados de verdade. Fazendo
    // rodizio, cada letra tem a mesma chance.
    int m_NextItemSlot;

    void UseItem(int itemType);
};
extern CAutoPotion gAutoPotion;