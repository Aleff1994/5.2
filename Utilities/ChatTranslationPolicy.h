#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace ChatTranslationPolicy {
const unsigned char Subcode=0x6B;
inline bool Language(unsigned value){return value<3;}
inline const char* Code(int value){return value==0?"pt":value==1?"en":value==2?"es":"auto";}
inline int PrefixLength(const std::string& text){
 if(text.empty())return 0;
 if(text[0]=='@')return text.size()>1 && text[1]=='@'?2:1;
 return text[0]=='~' || text[0]=='$'?1:0;
}
inline bool Eligible(const std::string& text){
 if(text.empty() || text[0]=='/' || text.find('\0')!=std::string::npos || text.find('\n')!=std::string::npos || text.find('\r')!=std::string::npos)return false;
 if(text.size()>1 && text[0]=='@' && text[1]=='>')return false; // Guild notices have their own path.
 const int prefix=PrefixLength(text);bool letters=false;
 for(size_t i=prefix;i<text.size();++i){unsigned char c=static_cast<unsigned char>(text[i]);if(c<32)return false;if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c>=192)letters=true;}
 return letters;
}
inline bool DecodeLanguage(const unsigned char* bytes,int size,int& language){
 if(!bytes || size!=6 || bytes[0]!=0xC1 || bytes[1]!=6 || bytes[2]!=0xF3 || bytes[3]!=Subcode || bytes[4]!=1 || !Language(bytes[5]))return false;
 language=bytes[5];return true;
}
inline bool SessionMatches(unsigned long expectedTick,const std::string& expectedAccount,const std::string& expectedName,
 unsigned long currentTick,const std::string& currentAccount,const std::string& currentName){
 return expectedTick==currentTick && expectedAccount==currentAccount && expectedName==currentName;
}
}
