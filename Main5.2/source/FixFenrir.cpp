#include "stdafx.h"
#include <Windows.h>

void FixPetTwistSlash()
{
    DWORD OldProtect;
    BYTE* addr = nullptr;

    addr = (BYTE*)0x0057C8F1;
    if (*(WORD*)addr == 0x840F) {
        VirtualProtect(addr, 2, PAGE_EXECUTE_READWRITE, &OldProtect);
        *(WORD*)addr = 0x9090;
        VirtualProtect(addr, 2, OldProtect, &OldProtect);
        return;
    }

    addr = (BYTE*)0x0057CB50;
    if (*(WORD*)addr == 0x840F) {
        VirtualProtect(addr, 2, PAGE_EXECUTE_READWRITE, &OldProtect);
        *(WORD*)addr = 0x9090;
        VirtualProtect(addr, 2, OldProtect, &OldProtect);
        return;
    }

    addr = (BYTE*)0x0057D2B4;
    if (*(WORD*)addr == 0x840F) {
        VirtualProtect(addr, 2, PAGE_EXECUTE_READWRITE, &OldProtect);
        *(WORD*)addr = 0x9090;
        VirtualProtect(addr, 2, OldProtect, &OldProtect);
        return;
    }
}