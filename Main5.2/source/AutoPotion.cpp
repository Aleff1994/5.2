#include "stdafx.h"
#include "AutoPotion.h"
#include "NewUISystem.h"
#include "wsclientinline.h"
#include "zzzinterface.h"
#include "zzzinventory.h"
#include "WINHANDLE.h"
// Nota: g_hWnd nao existe nesse projeto. O handle da janela e acessado
// atraves de gwinhandle->GethWnd() (mesmo padrao usado em outros
// lugares, ex: ImmGetContext(gwinhandle->GethWnd())). gwinhandle e
// global, declarado em WINHANDLE.h/.cpp.

CAutoPotion gAutoPotion;

CAutoPotion::CAutoPotion()
{
    this->m_Thread = NULL;
    this->m_LastUseTick = 0;      // controla cooldown entre disparos de UseItem
    this->m_WantsUsePotion = false; // NOVO: flag de intencao, lida pela thread principal
    this->m_NextItemSlot = 1;       // NOVO: comeca pelo Q, depois roda W/E/R
    this->CreateAutoPotion();
}

CAutoPotion::~CAutoPotion()
{
    if (this->m_Thread != NULL)
    {
        TerminateThread(this->m_Thread, 0);
        CloseHandle(this->m_Thread);
        this->m_Thread = NULL;
    }
}

void CAutoPotion::CreateAutoPotion()
{
    if (this->m_Thread == NULL)
    {
        this->m_Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)this->AutoPotionWorker, this, 0, 0);
    }
}

DWORD WINAPI CAutoPotion::AutoPotionWorker(CAutoPotion* AutoPotion)
{
    bool f6Last = false;
    bool lastf6Last = false;

    // NOVO: cooldown minimo (ms) entre ciclos de uso de item, pra nao floodar o servidor
    const DWORD USE_ITEM_COOLDOWN_MS = 500;

    while (1)
    {
        Sleep(10);
        if (GetForegroundWindow() != gwinhandle->GethWnd())
            continue;

        if (AutoPotion->m_Potion &&
            (g_pNewUISystem->IsVisible(7) || g_pNewUISystem->IsVisible(8)))
        {
            AutoPotion->m_Potion = false;
            char Text[256];
            sprintf(Text, "Auto Potion Desativado (interface visível)");
            g_pChatListBox->AddText("", Text, SEASON3B::TYPE_SYSTEM_MESSAGE);
            lastf6Last = false;
        }

        bool f6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
        if (f6 && !f6Last)
        {
            AutoPotion->m_Potion = !AutoPotion->m_Potion;
            char Text[256];
            sprintf(Text, "Auto Potion %s", AutoPotion->m_Potion ? "Ativado" : "Desativado");
            g_pChatListBox->AddText("", Text, SEASON3B::TYPE_SYSTEM_MESSAGE);
            lastf6Last = AutoPotion->m_Potion;
        }
        f6Last = f6;

        if (AutoPotion->m_Potion)
        {
            // MUDANCA IMPORTANTE: esta thread NAO manda mais pacote nenhum.
            // Ela so sinaliza a intencao (m_WantsUsePotion = true). Quem realmente
            // chama UseItem/SendRequestUse e' a thread principal, dentro de
            // ProcessMainThread(), evitando que duas threads escrevam no socket
            // ao mesmo tempo (essa concorrencia era a causa real do disconnect).
            AutoPotion->m_WantsUsePotion = true;
        }
    }
    return 0;
}

void CAutoPotion::ProcessMainThread()
{
    // NOVO METODO. Deve ser chamado SOMENTE pela thread principal do jogo
    // (dentro de winLoop, perto de ProtocolCompiler()). Nunca chamar isso
    // de dentro de AutoPotionWorker ou de qualquer outra thread.

    // IMPORTANTE: removido o cooldown artificial (200ms/500ms) que eu tinha
    // colocado antes. Ele nao era necessario pra resolver o disconnect -
    // o problema real era duas threads mexendo no socket ao mesmo tempo,
    // que ja foi resolvido movendo o envio pra ca (thread principal).
    //
    // A velocidade de uso real ja e' limitada pelo proprio jogo via
    // EnableUse (em ProtocolSend.h: SendRequestUse so envia se
    // EnableUse <= 0, e EnableUse so volta a 0 quando o SERVIDOR confirma
    // o uso anterior). Ou seja, o gargalo real e' o seu ping, nao um timer
    // nosso - por isso nao faz sentido esperar um tempo fixo aqui: isso so
    // atrasava desnecessariamente quando o servidor ja tinha liberado.
    //
    // Ao tirar o timer artificial, voltamos a "tentar toda vez que der",
    // igual o original fazia - so que agora sempre pela thread principal,
    // que e' a parte que realmente importava pra seguranca.

    if (!this->m_WantsUsePotion)
        return;

    this->m_WantsUsePotion = false;

    // MUDANCA IMPORTANTE: antes tentava Q, W, E, R nessa ordem fixa TODA VEZ.
    // Como o jogo so libera um uso por vez (EnableUse trava ate o servidor
    // confirmar - ver SendRequestUse em ProtocolSend.h), o Q sempre ganhava
    // a vez de novo antes de W/E/R conseguirem passar. Resultado: so o Q
    // funcionava na pratica, mesmo com os 4 "tentando".
    //
    // Agora tentamos UM item por ciclo, em rodizio (Q -> W -> E -> R -> Q...).
    // Isso garante que toda letra tenha chance real de ser usada, mantendo
    // a mesma velocidade total (o gargalo continua sendo o ping, como
    // expliquei antes - rodizio nao deixa mais lento, so' reparte melhor).
    this->UseItem(this->m_NextItemSlot);

    this->m_NextItemSlot++;
    if (this->m_NextItemSlot > 3) // ajuste pra 4 se/quando reativar o R
        this->m_NextItemSlot = 1;
}

void CAutoPotion::UseItem(int itemType)
{
    int iIndex = -1;
    if (itemType == 1) {
        SEASON3B::CNewUIItemHotKey hotKeyInstance;
        iIndex = hotKeyInstance.GetHotKeyItemIndex(SEASON3B::HOTKEY_Q);
    }
    else if (itemType == 2) {
        SEASON3B::CNewUIItemHotKey hotKeyInstance;
        iIndex = hotKeyInstance.GetHotKeyItemIndex(SEASON3B::HOTKEY_W);
    }
    else if (itemType == 3) {
        SEASON3B::CNewUIItemHotKey hotKeyInstance;
        iIndex = hotKeyInstance.GetHotKeyItemIndex(SEASON3B::HOTKEY_E);
    }
    else if (itemType == 4) { // NOVO
        SEASON3B::CNewUIItemHotKey hotKeyInstance;
        iIndex = hotKeyInstance.GetHotKeyItemIndex(SEASON3B::HOTKEY_R);
    }

    if (iIndex != -1)
    {
        ITEM* pItem = NULL;
        pItem = g_pMyInventory->FindItem(iIndex);

        // NOVO: checagem de NULL antes de acessar pItem->Type.
        // FindItem pode retornar NULL (slot vazio, item consumido nesse instante
        // por outra thread, etc). Sem essa checagem, o acesso a pItem->Type
        // e um acesso de memoria invalido -> crash do client, que pode aparecer
        // pro jogador como um "disconnect".
        if (pItem == NULL)
            return;

        if ((pItem->Type >= ITEM_POTION + 78 && pItem->Type <= ITEM_POTION + 82))
        {
            std::list<eBuffState> secretPotionbufflist;
            secretPotionbufflist.push_back(eBuff_SecretPotion1);
            secretPotionbufflist.push_back(eBuff_SecretPotion2);
            secretPotionbufflist.push_back(eBuff_SecretPotion3);
            secretPotionbufflist.push_back(eBuff_SecretPotion4);
            secretPotionbufflist.push_back(eBuff_SecretPotion5);
            if (g_isCharacterBufflist((&Hero->Object), secretPotionbufflist) != eBuffNone) {
                SEASON3B::CreateOkMessageBox(GlobalText[2530], RGBA(255, 30, 0, 255));
            }
            else {
                SendRequestUse(iIndex, 0);
            }
        }
        else
        {
            SendRequestUse(iIndex, 0);
        }
    }
}