#include "../Utilities/IndependentTierCacheCore.h"
#include <cassert>
#include <cstdio>
using namespace IndependentTierProtocol;
struct Backend {
 bool enabled=true,fail=false;std::uint32_t time=100,next=0;
 std::map<std::uint32_t,Message> sent,replies;
 std::uint32_t Now() const { return time; }bool Enabled() const { return enabled; }
 std::uint32_t Send(Message m) { if(fail)return 0;sent[++next]=m;return next; }
 bool Take(std::uint32_t id,Message& m) {auto i=replies.find(id);if(i==replies.end()) return false;m=i->second;replies.erase(i);return true;}
};
int main() {
 using namespace IndependentTierCache;
 Backend b;Cache<Backend> cache(b);
 for(unsigned i=1;i<=33;++i) {cache.Request(i,4096);cache.Request(i,4096);}
 cache.Pump();assert(b.sent.size()==3);assert(b.sent[1].entries.size()==16);assert(b.sent[3].entries.size()==1);
 IndependentItemTiers::Metadata m;
 assert(cache.Lookup(1,4096,m)==State::Pending);
 for(auto pair:b.sent) {
  Message response=pair.second;response.operation=Operation::ReadResult;
  for(auto& e:response.entries) {e.status=e.serial==1?Status::Found:e.serial==2?Status::Error:Status::Missing;e.itemId=4096;e.metadata.itemGrade=IndependentItemTiers::Grade::B;}
  b.replies[pair.first]=response;
 }
 cache.Pump();assert(cache.Lookup(1,4096,m)==State::Ready);assert(m.itemGrade==IndependentItemTiers::Grade::B);
 const auto ready=cache.TakeReady();assert(ready.size()==1 && ready[0]==1);assert(cache.TakeReady().empty());
 assert(cache.Lookup(1,4097,m)==State::Error);assert(cache.Lookup(2,4096,m)==State::Error);assert(cache.Lookup(3,4096,m)==State::Legacy);
 cache.Request(1,4096);cache.Request(2,4096);cache.Pump();assert(b.sent.size()==3);
 b.time+=5001;cache.Request(2,4096);cache.Pump();assert(b.sent.size()==4);
 b.time+=30001;cache.Pump();assert(cache.Lookup(2,4096,m)==State::Error);
 cache.Clear();assert(cache.Lookup(1,4096,m)==State::Unknown);
 b.enabled=false;cache.Request(44,4096);assert(cache.Lookup(44,4096,m)==State::Unknown);
 b.enabled=true;b.fail=true;cache.Request(45,4096);cache.Pump();assert(cache.Lookup(45,4096,m)==State::Queued);
 b.fail=false;cache.Pump();assert(cache.Lookup(45,4096,m)==State::Pending);
 // Automatic retry does not require a new item move/request.
 Backend retryBackend;Cache<Backend> retryCache(retryBackend);retryCache.Request(80,4096);retryCache.Pump();
 auto errorReply=retryBackend.sent[1];errorReply.operation=Operation::ReadResult;errorReply.entries[0].status=Status::Error;
 retryBackend.replies[1]=errorReply;retryCache.Pump();retryBackend.time+=5001;retryCache.Pump();retryCache.Pump();
 assert(retryBackend.sent.size()==2);
 // Keep equipped metadata alive while idle bag records can expire.
 Backend ttlBackend;Cache<Backend> ttlCache(ttlBackend);ttlCache.Request(90,4096);ttlCache.Pump();
 auto foundReply=ttlBackend.sent[1];foundReply.operation=Operation::ReadResult;foundReply.entries[0].status=Status::Found;foundReply.entries[0].itemId=4096;
 ttlBackend.replies[1]=foundReply;ttlCache.Pump();ttlBackend.time+=599000;ttlCache.Touch(90);ttlBackend.time+=2000;ttlCache.Pump();ttlCache.Pump();
 assert(ttlCache.Lookup(90,4096,m)==State::Ready);
 ttlBackend.time+=600001;ttlCache.Pump();ttlCache.Pump();assert(ttlCache.Lookup(90,4096,m)==State::Unknown);
 puts("PASS: deduplication, 16-item batching, identity mismatch, Missing vs Error, retries, timeout, disconnect clear, disabled and send failure.");
}
