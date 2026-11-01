#include "StdAfx.h"
#include "Util.h"
#include "MemScript.h"
#include "CEventName.h"

CEventName  gEventName;

CEventName::CEventName() : m_EventGlobal(MAX_SIZE_EVENT_TEMPLATE),
m_InvasionGlobal(MAX_SIZE_EVENT_TEMPLATE),
m_ArenaGlobal(MAX_SIZE_EVENT_TEMPLATE)
{
}

CEventName::~CEventName()
{
	m_EventGlobal.clear();
	m_ArenaGlobal.clear();
	m_InvasionGlobal.clear();
}

void CEventName::Release()
{
	m_EventGlobal.clear();
	m_ArenaGlobal.clear();
	m_InvasionGlobal.clear();

	m_EventGlobal.resize(MAX_SIZE_EVENT_TEMPLATE);
	m_ArenaGlobal.resize(MAX_SIZE_EVENT_TEMPLATE);
	m_InvasionGlobal.resize(MAX_SIZE_EVENT_TEMPLATE);
}

void CEventName::OpenFile(std::string filename)
{
	CMemScript* lpMemScript = new CMemScript;
	if (lpMemScript == 0 || lpMemScript->SetBuffer(filename.c_str()) == 0)
	{
		ErrorMessageBox((lpMemScript == 0) ? MEM_SCRIPT_ALLOC_ERROR : lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}

	this->Release();
	this->m_SendClientDataEventTime.clear();

	try
	{
		while (true)
		{
			if (lpMemScript->GetToken() == TOKEN_END) break;
			int switch_on = lpMemScript->GetNumber();
			while (true)
			{
				if (lpMemScript->GetToken() == TOKEN_END) break;
				if (strcmp("end", lpMemScript->GetString()) == 0) break;
				TEMPLATE_EVENT info;
				info.switch_on = switch_on;
				info.m_Key = lpMemScript->GetNumber();
				info.m_Name = lpMemScript->GetAsString();
				info.m_Des = lpMemScript->GetAsString();
				info.m_Gate = lpMemScript->GetAsNumber();
				this->push_back(switch_on, info);
				this->m_SendClientDataEventTime.push_back(info);
			}
		}
	}
	catch (...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
}
void CEventName::push_back(int switch_on, const TEMPLATE_EVENT& info)
{
	if (info.m_Key >= MAX_SIZE_EVENT_TEMPLATE)
		return;

	switch (switch_on)
	{
	case 0:
		m_EventGlobal[info.m_Key] = (info);
		break;
	case 1:
		m_InvasionGlobal[info.m_Key] = (info);
		break;
	case 2:
		m_ArenaGlobal[info.m_Key] = (info);
		break;
	default:
		break;
	}
}
int CEventName::GetTimeEventSwitch(int switch_on, int Index)
{
	int RemainTime = -1;

	switch (switch_on)
	{
	case 0:
		RemainTime = this->GlobalRemainTime(Index);
		break;
	case 1:
		RemainTime = this->InvasionRemainTime(Index);
		break;
	case 2:
		RemainTime = this->ArenaRemainTime(Index);
		break;
	default:
		break;
	}
	return RemainTime;
}

bool CEventName::IsArena(int Index)
{
	return (Index < MAX_SIZE_EVENT_TEMPLATE && m_ArenaGlobal[Index].m_Key != -1);
}

bool CEventName::IsGlobal(int Index)
{
	return (Index < MAX_SIZE_EVENT_TEMPLATE && m_EventGlobal[Index].m_Key != -1);
}

bool CEventName::IsInvasion(int Index)
{
	return (Index < MAX_SIZE_EVENT_TEMPLATE && m_InvasionGlobal[Index].m_Key != -1);
}

const char* CEventName::ArenaName(int Index)
{
	if (Index < MAX_SIZE_EVENT_TEMPLATE)
	{
		return m_ArenaGlobal[Index].GetName();
	}

	return "No Name";
}

const char* CEventName::GlobalName(int Index)
{
	if (Index < MAX_SIZE_EVENT_TEMPLATE)
	{
		return m_EventGlobal[Index].GetName();
	}

	return "No Name";
}

const char* CEventName::InvasionName(int Index)
{
	if (Index < MAX_SIZE_EVENT_TEMPLATE)
	{
		return m_InvasionGlobal[Index].GetName();
	}

	return "No Name";
}

int CEventName::ArenaRemainTime(int Index)
{
	int RemainTime = -1;

	if (Index < MAX_SIZE_EVENT_TEMPLATE && m_ArenaGlobal[Index].m_Key != -1)
	{
		RemainTime = m_ArenaGlobal[Index].m_RemainTime;
	}

	return RemainTime;
}

int CEventName::GlobalRemainTime(int Index)
{
	int RemainTime = -1;

	if (Index < MAX_SIZE_EVENT_TEMPLATE && m_EventGlobal[Index].m_Key != -1)
	{
		RemainTime = m_EventGlobal[Index].m_RemainTime;
	}

	return RemainTime;
}

int CEventName::InvasionRemainTime(int Index)
{
	int RemainTime = -1;

	if (Index < MAX_SIZE_EVENT_TEMPLATE && m_EventGlobal[Index].m_Key != -1)
	{
		RemainTime = m_InvasionGlobal[Index].m_RemainTime;
	}

	return RemainTime;
}

void CEventName::ArenaRemainTime(int Index, int Time)
{
	if (Index < MAX_SIZE_EVENT_TEMPLATE)
	{
		m_ArenaGlobal[Index].m_RemainTime = Time;
	}
}

void CEventName::GlobalRemainTime(int Index, int Time)
{
	if (Index < MAX_SIZE_EVENT_TEMPLATE)
	{
		m_EventGlobal[Index].m_RemainTime = Time;
	}
}

void CEventName::InvasionRemainTime(int Index, int Time)
{
	if (Index < MAX_SIZE_EVENT_TEMPLATE)
	{
		m_InvasionGlobal[Index].m_RemainTime = Time;
	}
}
