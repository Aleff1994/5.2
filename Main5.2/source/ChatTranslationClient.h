#pragma once
#include "StreamPacketEngine.h"
#include "../../Utilities/ChatTranslationPolicy.h"
namespace ChatTranslationClient {
inline void Tick(){
 if(SceneFlag!=MAIN_SCENE)return;
 int language=-1;
 if(_stricmp(g_strSelectedML.c_str(),"Por")==0)language=0;
 else if(_stricmp(g_strSelectedML.c_str(),"Eng")==0)language=1;
 else if(_stricmp(g_strSelectedML.c_str(),"Spn")==0)language=2;
 if(language<0)return;
 static int lastLanguage=-1;static DWORD last=0;const DWORD now=GetTickCount();
 if(lastLanguage==language && last && now-last<2000)return;
 CStreamPacketEngine packet;packet.Init(0xC1,0xF3);
 packet<<static_cast<BYTE>(ChatTranslationPolicy::Subcode)<<static_cast<BYTE>(1)<<static_cast<BYTE>(language);packet.Send();
 lastLanguage=language;last=now;
}
}
