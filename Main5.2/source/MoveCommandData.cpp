// MoveCommandData.cpp: implementation of the CMoveCommandData class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ZzzInfomation.h"
#include "MoveCommandData.h"

using namespace SEASON3B;

CMoveCommandData::CMoveCommandData()
{
}

CMoveCommandData::~CMoveCommandData()
{
	Release();
}

CMoveCommandData* CMoveCommandData::GetInstance()
{
	static CMoveCommandData s_Instance;
	return &s_Instance;
}

bool CMoveCommandData::Create(const std::string& filename)
{
	FILE* fp = fopen ( filename.c_str(), "rb" );
	if(fp == NULL) return false;

	int count = 0;
	fread(&count, sizeof(int), 1, fp);
	
	for(int i=0; i<count; i++)
	{		
		MOVEINFODATA* pMoveInfoData = new MOVEINFODATA;
		fread(&(pMoveInfoData->_ReqInfo), sizeof(MOVEREQINFO), 1, fp);
		BuxConvert((BYTE*)&(pMoveInfoData->_ReqInfo), sizeof(MOVEREQINFO));	
		m_listMoveInfoData.push_back(pMoveInfoData);
	}
	fclose(fp);

	return true;
}

void CMoveCommandData::Release()
{
	std::list<MOVEINFODATA*>::iterator li = m_listMoveInfoData.begin();
	for(; li != m_listMoveInfoData.end(); li++)
		delete (*li);
	m_listMoveInfoData.clear();
}

void CMoveCommandData::BuxConvert(BYTE* pBuffer, int size)
{
	BYTE bBuxCode[3] = {0xfc,0xcf,0xab};
	for(int i=0;i<size;i++)
		pBuffer[i] ^= bBuxCode[i%3];
}

bool CMoveCommandData::OpenMoveReqScript(const std::string& filename) 
{
	return CMoveCommandData::GetInstance()->Create(filename);
}

int CMoveCommandData::GetNumMoveMap()
{	
	if( m_listMoveInfoData.size() > 0 )
		return m_listMoveInfoData.size();

	return -1;
}

const CMoveCommandData::MOVEINFODATA* CMoveCommandData::GetMoveCommandDataByIndex( int iIndex )
{
	std::list<MOVEINFODATA*>::iterator li = m_listMoveInfoData.begin();
	for(; li != m_listMoveInfoData.end(); li++)
	{
		if( (*li)->_ReqInfo.index == iIndex )
		{
			return (*li);
		}
	}
	return 0;
}

const std::list<CMoveCommandData::MOVEINFODATA*>& CMoveCommandData::GetMoveCommandDatalist()
{
	return m_listMoveInfoData;
}



bool SEASON3B::CMoveCommandData::ReloadNames(const std::string& filename)
{
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp) return false;
    int count = 0;
    bool valid = fread(&count, sizeof(count), 1, fp) == 1 && count > 0 && count <= 4096;
    fseek(fp, 0, SEEK_END);
    valid = valid && ftell(fp) == (long)(sizeof(count) + count * sizeof(MOVEREQINFO));
    fseek(fp, sizeof(count), SEEK_SET);
    std::vector<MOVEREQINFO> translated(valid ? count : 0);
    if (valid) valid = fread(translated.data(), sizeof(MOVEREQINFO), count, fp) == (size_t)count;
    fclose(fp);
    if (!valid) return false;
    for (auto& row : translated) GetInstance()->BuxConvert((BYTE*)&row, sizeof(row));
    // The UI owns pointers to these rows. Update names in place and keep the
    // sub-map command, gate, prices and requirements unchanged.
    for (MOVEINFODATA* current : GetInstance()->m_listMoveInfoData)
        for (const auto& row : translated)
            if (current->_ReqInfo.index == row.index)
            {
                memcpy(current->_ReqInfo.szMainMapName, row.szMainMapName, sizeof(row.szMainMapName));
                current->_ReqInfo.szMainMapName[sizeof(row.szMainMapName) - 1] = 0;
                break;
            }
    return true;
}
