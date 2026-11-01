#pragma once
#include "IndependentTierView.h"
#include "../../Utilities/ExcellentEquippedProtocol.h"
namespace ExcellentEquippedView {
struct View {
 ExcellentEquipped::Snapshot snapshot;DWORD nonce=0,sent=0,received=0;
 void Reset(){snapshot={};sent=received=0;++nonce;}
 void Poll(){const DWORD now=GetTickCount();if(sent && now-sent<1000)return;sent=now;if(++nonce==0)++nonce;
  auto bytes=ExcellentEquipped::Request(nonce);CStreamPacketEngine packet;packet.Init(0xC1,0xF3);packet.AddData(bytes.data()+3,static_cast<WORD>(bytes.size()-3));packet.Send();}
 void Receive(const BYTE* bytes,int size){ExcellentEquipped::Snapshot result;if(size>=0 && sent && GetTickCount()-sent<3000 && ExcellentEquipped::Decode(bytes,static_cast<unsigned>(size),nonce,result)){snapshot=result;received=GetTickCount();}}
 bool Fresh()const{return received && GetTickCount()-received<2500;}
 void Render(int x,int y) {
  int count=0;
  auto line=[&](const char* text,int color,bool bold=false){strcpy_s(TextList[count],sizeof(TextList[count]),text);TextListColor[count]=color;TextBold[count++]=bold;};
  line(gTextClien.Select("EXCELLENT EQUIPADOS","EQUIPPED EXCELLENT","EXCELLENT EQUIPADOS"),TEXT_COLOR_WHITE,true);
  if(!Fresh())line(gTextClien.Select("Carregando adicionais equipados...","Loading equipped options...","Cargando opciones equipadas..."),TEXT_COLOR_GRAY);
  else if(!snapshot.enabled)line(gTextClien.Select("Sistema de ranks desativado","Rank system disabled","Sistema de rangos desactivado"),TEXT_COLOR_GRAY);
  else {
   char text[256];sprintf_s(text,gTextClien.Select("Pecas com ranks ativos: %u","Pieces with active ranks: %u","Piezas con rangos activos: %u"),snapshot.pieces);line(text,TEXT_COLOR_WHITE);
   const char* names[]={"RARE","LEGENDARY","EPIC"};const int colors[]={ExcellentRankStyle::Rare,ExcellentRankStyle::Legendary,ExcellentRankStyle::Epic};
   for(unsigned i=0;i<3;++i){sprintf_s(text,"[ %s ]  %u",names[i],snapshot.ranks[i]);line(text,colors[i],true);}
   if(snapshot.pending)line(gTextClien.Select("Leitura pendente: resumo parcial","Pending data: partial summary","Datos pendientes: resumen parcial"),TEXT_COLOR_YELLOW);
   line(gTextClien.Select("Soma dos adicionais equipados:","Sum of equipped options:","Suma de opciones equipadas:"),TEXT_COLOR_WHITE,true);
   for(unsigned i=0;i<ExcellentEquipped::Count;++i)if(snapshot.values[i]){text[0]=0;IndependentTierView::FormatResolvedOption(text,sizeof(text),ExcellentEquipped::Kinds[i],snapshot.values[i]);if(text[0])line(text,TEXT_COLOR_BLUE);}
   if(!snapshot.pieces && !snapshot.pending)line(gTextClien.Select("Nenhum adicional Rare/Legendary/Epic equipado","No Rare/Legendary/Epic options equipped","Sin opciones Rare/Legendary/Epic equipadas"),TEXT_COLOR_GRAY);
   line(gTextClien.Select("Dano contra monstros e cura ao matar: so PvM","Monster damage and life on kill: PvM only","Dano a monstruos y cura al matar: solo PvM"),TEXT_COLOR_GRAY);
   line(gTextClien.Select("Soma das opcoes; limites de cada efeito se aplicam","Option totals; each effect has its own limits","Suma de opciones; aplican los limites de cada efecto"),TEXT_COLOR_GRAY);
  }
  // Native tooltip coordinates; anchor beside inventory, within the game viewport.
  RenderTipTextList(x>260?x-130:160,y+42,count,240,RT3_SORT_CENTER);
 }
};
inline View& Get(){static View view;return view;}
}
