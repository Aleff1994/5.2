#pragma once
#include "User.h"
#include "Util.h"
#include "../../Utilities/ChatTranslationPolicy.h"
#include "../../Utilities/ChatTranslationTransport.h"
#include <deque>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
namespace ChatTranslation {
const UINT WakeMessage=WM_APP+0x36B;
inline const char* CommonPath(){return "..\\4.- GameServer\\Data\\GameServerInfo - Common.dat";}
struct LanguageState {DWORD tick;std::string account,name;int language;};
struct Job {int user,language,source,timeout;unsigned short port;DWORD tick,queued;std::string account,name,sender,text;std::vector<BYTE> packet;};
struct Result {Job job;std::string translated;};
class Runtime {
 std::mutex lock;std::condition_variable condition;std::map<int,LanguageState> languages;
 std::deque<Job> queue;std::deque<Result> done;unsigned outstanding=0;
 bool enabled=false;unsigned short port=55890;int timeout=2000;DWORD settingsRead=0;HWND window=NULL;
 std::atomic<bool> active{false};
 struct Cached {DWORD tick;std::string translated;};
 void ReadSettings(){
  const DWORD now=GetTickCount();if(settingsRead && now-settingsRead<1000)return;settingsRead=now;
  enabled=GetPrivateProfileIntA("GameServerInfo","ChatTranslateEnable",0,CommonPath())==1;
  int configured=GetPrivateProfileIntA("GameServerInfo","ChatTranslatePort",55890,CommonPath());port=static_cast<unsigned short>(configured>=1024 && configured<=65535?configured:55890);
  configured=GetPrivateProfileIntA("GameServerInfo","ChatTranslateTimeoutMs",2000,CommonPath());timeout=configured>=250 && configured<=5000?configured:2000;
  active.store(enabled);
 }
 bool Current(int user,const LanguageState& state){
  return OBJECT_USER_RANGE(user) && gObjIsConnectedGP(user) && ChatTranslationPolicy::SessionMatches(state.tick,state.account,state.name,gObj[user].ConnectTickCount,gObj[user].Account,gObj[user].Name);
 }
 void Worker(){
  std::map<std::string,Cached> cache;DWORD retryAt=0;
  for(;;){
   Job job;
   {std::unique_lock<std::mutex> guard(lock);condition.wait(guard,[this]{return !queue.empty();});job=std::move(queue.front());queue.pop_front();}
   std::string translated;
   const DWORD now=GetTickCount();
   const std::string key=std::to_string(job.source)+":"+std::to_string(job.language)+":"+job.sender+":"+job.text;
   const auto found=cache.find(key);
   if(active.load() && now-job.queued<4000){
    if(found!=cache.end() && now-found->second.tick<600000)translated=found->second.translated;
    else if(!retryAt || static_cast<LONG>(now-retryAt)>=0){
     const int prefix=ChatTranslationPolicy::PrefixLength(job.text);
     std::string value;
     if(ChatTranslationTransport::Translate(job.port,job.timeout,ChatTranslationPolicy::Code(job.source),ChatTranslationPolicy::Code(job.language),job.sender,job.text.substr(prefix),value)){
      translated=job.text.substr(0,prefix)+value;
      if(cache.size()>=512)cache.clear();cache[key]={GetTickCount(),translated};retryAt=0;
     }else retryAt=GetTickCount()+5000;
    }
   }
   HWND notify=NULL;
   {std::lock_guard<std::mutex> guard(lock);done.push_back({std::move(job),std::move(translated)});notify=window;}
   if(notify)PostMessageA(notify,WakeMessage,0,0);
  }
 }
public:
 Runtime(){ReadSettings();std::thread([this]{Worker();}).detach();}
 void Register(int user,const BYTE* bytes,int size){
  int language=-1;if(!ChatTranslationPolicy::DecodeLanguage(bytes,size,language) || !OBJECT_USER_RANGE(user) || !gObjIsConnectedGP(user))return;
  std::lock_guard<std::mutex> guard(lock);
  languages[user]={gObj[user].ConnectTickCount,gObj[user].Account,gObj[user].Name,language};
 }
 void Deliver(int user,const BYTE* bytes,int size){
  if(!OBJECT_USER_RANGE(user) || !gObjIsConnectedGP(user))return;
  if(size<14 || size>255 || bytes[0]!=0xC1 || (bytes[2]!=0 && bytes[2]!=2)){DataSend(user,const_cast<BYTE*>(bytes),size);return;}
  const std::string text(reinterpret_cast<const char*>(bytes+13),strnlen_s(reinterpret_cast<const char*>(bytes+13),size-13));
  if(!ChatTranslationPolicy::Eligible(text)){DataSend(user,const_cast<BYTE*>(bytes),size);return;}
  char sender[11]={};memcpy(sender,bytes+3,10);
  bool queued=false;
  {std::lock_guard<std::mutex> guard(lock);
   auto target=languages.find(user);
   if(enabled && target!=languages.end() && Current(user,target->second) && outstanding<128 && strcmp(sender,gObj[user].Name)!=0){
    int source=3;LPOBJ origin=gObjFind(sender);
    if(origin && OBJECT_USER_RANGE(origin->Index)){
     auto lang=languages.find(origin->Index);if(lang!=languages.end() && Current(origin->Index,lang->second))source=lang->second.language;
    }
    if(source!=target->second.language){
     const auto& state=target->second;
     queue.push_back({user,state.language,source,timeout,port,state.tick,GetTickCount(),state.account,state.name,sender,text,std::vector<BYTE>(bytes,bytes+size)});
     ++outstanding;queued=true;
    }
   }
  }
  if(queued)condition.notify_one();else DataSend(user,const_cast<BYTE*>(bytes),size);
 }
 void Tick(HWND owner){
  std::deque<Result> completed;
  {std::lock_guard<std::mutex> guard(lock);window=owner;ReadSettings();completed.swap(done);outstanding-=static_cast<unsigned>(completed.size());}
  for(auto& result:completed){
   Job& job=result.job;
   if(!OBJECT_USER_RANGE(job.user) || !gObjIsConnectedGP(job.user) || !ChatTranslationPolicy::SessionMatches(job.tick,job.account,job.name,gObj[job.user].ConnectTickCount,gObj[job.user].Account,gObj[job.user].Name))continue;
   bool translate=false;
   {std::lock_guard<std::mutex> guard(lock);auto language=languages.find(job.user);translate=enabled && language!=languages.end() && language->second.language==job.language;}
   if(translate && !result.translated.empty()){
    // CP1252 is single-byte for these languages; retain the client's chat bound.
    if(result.translated.size()>MAX_CHAT_MESSAGE_SIZE-1)result.translated.resize(MAX_CHAT_MESSAGE_SIZE-1);
    std::vector<BYTE> packet(job.packet.begin(),job.packet.begin()+13);
    packet.insert(packet.end(),result.translated.begin(),result.translated.end());packet.push_back(0);packet[1]=static_cast<BYTE>(packet.size());
    DataSend(job.user,packet.data(),static_cast<int>(packet.size()));
   }else DataSend(job.user,job.packet.data(),static_cast<int>(job.packet.size()));
  }
 }
};
inline Runtime& Get(){static Runtime* runtime=new Runtime;return *runtime;}
inline void Deliver(int user,const BYTE* bytes,int size){Get().Deliver(user,bytes,size);}
}
