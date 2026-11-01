#include <vector>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <cstdio>
using std::min;
typedef unsigned char BYTE;typedef unsigned short WORD;
const int OBJECT_START_MONSTER=0,MAX_OBJECT_MONSTER=16,OBJECT_NPC=2,OBJECT_OFFLINE=0;
struct Obj{int Type,Connected,Live,Map,X,Y,Class;};typedef Obj* LPOBJ;Obj gObj[17];
struct Header{BYTE dummy;void set(int,int,int){}};
struct PMSG_DYNAMIC_MINIMAP_HEAD{Header header;BYTE map,reset;WORD count;};
struct PMSG_DYNAMIC_MINIMAP_ENTRY{BYTE kind,x,y,tx,ty;WORD id;char text[32];};
std::vector<PMSG_DYNAMIC_MINIMAP_ENTRY> captured;int packets=0;
void DataSend(int,BYTE* p,int size){auto h=(PMSG_DYNAMIC_MINIMAP_HEAD*)p;assert(h->reset==1);assert(h->count<=4);assert(size==sizeof(*h)+h->count*sizeof(PMSG_DYNAMIC_MINIMAP_ENTRY));auto e=(PMSG_DYNAMIC_MINIMAP_ENTRY*)(p+sizeof(*h));captured.assign(e,e+h->count);++packets;}
int gObjIsConnectedGP(int){return 1;}
struct Manager{const char* GetMonsterName(int){return "NPC";}}gMonsterManager;
class CMiniMap{public:void GCDynamicMapInfoSend(int);};
void CMiniMap::GCDynamicMapInfoSend(int aIndex)

{

	if (gObjIsConnectedGP(aIndex) == 0)

	{

		return;

	}



	std::vector<PMSG_DYNAMIC_MINIMAP_ENTRY> entries;

    // Only actual live NPCs. Spawn section numbers alone do not prove NPC type.

    // Keep a bounded nearest-four selection; no monster/player/portal markers.

    for(int n=OBJECT_START_MONSTER;n<MAX_OBJECT_MONSTER;++n)

    {

        const LPOBJ npc=&gObj[n];

        if(npc->Type!=OBJECT_NPC || npc->Connected==OBJECT_OFFLINE || !npc->Live || npc->Map!=gObj[aIndex].Map)continue;

        bool duplicate=false;

        for(size_t j=0;j<entries.size();++j)if(entries[j].x==npc->X && entries[j].y==npc->Y){duplicate=true;break;}

        if(duplicate)continue;

        const int dx=npc->X-gObj[aIndex].X,dy=npc->Y-gObj[aIndex].Y,distance=dx*dx+dy*dy;

        size_t at=0;

        for(;at<entries.size();++at){const int x=entries[at].x-gObj[aIndex].X,y=entries[at].y-gObj[aIndex].Y;if(distance<x*x+y*y)break;}

        if(at>=4)continue;

        PMSG_DYNAMIC_MINIMAP_ENTRY info={};info.kind=1;info.x=(BYTE)npc->X;info.y=(BYTE)npc->Y;info.tx=info.x;info.ty=info.y;info.id=(WORD)npc->Class;

        strncpy_s(info.text,gMonsterManager.GetMonsterName(npc->Class),_TRUNCATE);

        entries.insert(entries.begin()+at,info);if(entries.size()>4)entries.resize(4);

    }



	// Send every marker in legacy-safe chunks. A single packet with all map

	// records can exceed the client's 2 KB stream buffer and disappear entirely.

	const size_t chunkCapacity = 40;

	for (size_t offset = 0; offset < entries.size() || offset == 0; offset += chunkCapacity)

	{

		const size_t chunkCount = min(chunkCapacity, entries.size() - offset);

		const int size = sizeof(PMSG_DYNAMIC_MINIMAP_HEAD) + (int)(chunkCount * sizeof(PMSG_DYNAMIC_MINIMAP_ENTRY));

		BYTE* send = new BYTE[size];

		PMSG_DYNAMIC_MINIMAP_HEAD* head = (PMSG_DYNAMIC_MINIMAP_HEAD*)send;

		head->header.set(0xFA, 0x02, size);

		head->map = (BYTE)gObj[aIndex].Map;

		head->reset = (offset == 0) ? 1 : 0;

		head->count = (WORD)chunkCount;

		if (chunkCount != 0)

		{

			memcpy(send + sizeof(PMSG_DYNAMIC_MINIMAP_HEAD), &entries[offset], chunkCount * sizeof(PMSG_DYNAMIC_MINIMAP_ENTRY));

		}

		DataSend(aIndex, send, size);

		delete[] send;

	}

}




int main(){CMiniMap m;gObj[16]={3,1,1,0,0,0,0};m.GCDynamicMapInfoSend(16);assert(captured.empty());
int xs[]={5,1,3,4,2,6};for(int i=0;i<6;++i)gObj[i]={2,1,1,0,xs[i],0,100+i};
gObj[6]={3,1,1,0,0,0,200};gObj[7]={1,1,1,0,0,0,201};gObj[8]={2,1,1,1,0,0,202};gObj[9]={2,1,0,0,0,0,203};gObj[10]={2,0,1,0,0,0,204};gObj[11]={2,1,1,0,1,0,205};
m.GCDynamicMapInfoSend(16);assert(captured.size()==4);for(int i=0;i<4;++i){assert(captured[i].kind==1);assert(captured[i].x==i+1);}assert(captured[0].id==101);
for(int i=0;i<16;++i)gObj[i]={};for(int i=0;i<3;++i)gObj[i]={2,1,1,0,i+1,0,i};m.GCDynamicMapInfoSend(16);assert(captured.size()==3);
for(int i=0;i<16;++i)gObj[i]={1,1,1,0,0,0,i};m.GCDynamicMapInfoSend(16);assert(captured.empty());assert(packets==4);puts("PASS actual server minimap sender: empty, nearest-four, fewer-than-four, duplicates, live/type/map filters, clearing packet.");}