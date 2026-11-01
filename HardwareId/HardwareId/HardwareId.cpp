// Windows Header Files:
#include <windows.h>
#include <winbase.h>

#include <iostream>

#define WIN32_LEAN_AND_MEAN	

#define _CRT_SECURE_NO_WARNINGS

int main()
{
	SetConsoleTitle("HardwareId Generator");

	DWORD VolumeSerialNumber;

	if (GetVolumeInformation("C:\\", 0, 0, &VolumeSerialNumber, 0, 0, 0, 0) == 0)
	{
		while (true)
		{
			TerminateProcess(GetCurrentProcess(), 0);
		}
	}

	UUID uuid;

	UuidCreateSequential(&uuid);

	SYSTEM_INFO SystemInfo;

	GetSystemInfo(&SystemInfo);

	DWORD ComputerHardwareId1 = VolumeSerialNumber ^ 0x12B586FE;

	DWORD ComputerHardwareId2 = *(DWORD*)(&uuid.Data4[2]) ^ 0x5D78A569;

	DWORD ComputerHardwareId3 = ((*(WORD*)(&uuid.Data4[6]) & 0xFFFF) | (SystemInfo.wProcessorArchitecture << 16)) ^ 0xF45BC123;

	DWORD ComputerHardwareId4 = ((SystemInfo.wProcessorLevel & 0xFFFF) | (SystemInfo.wProcessorRevision << 16)) ^ 0xB542D8E1;

	char ComputerHardwareId[35] = { 0 };

	wsprintf(ComputerHardwareId, "%08X-%08X-%08X-%08X", ComputerHardwareId1, ComputerHardwareId2, ComputerHardwareId3, ComputerHardwareId4);


	HANDLE file = CreateFile("HardwareId.txt", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	DWORD OutSize = 0;

	if (WriteFile(file, &ComputerHardwareId, sizeof(ComputerHardwareId), &OutSize, 0) == 0)
	{
		CloseHandle(file);
		return 0;
	}

	CloseHandle(file);

	return 0;
}