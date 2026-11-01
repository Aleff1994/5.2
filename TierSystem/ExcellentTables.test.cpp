#include "../Utilities/IndependentExcellentTables.h"
#include <cassert>
#include <cstdio>
int main(){using namespace IndependentExcellentTables;using namespace IndependentItemTiers;Tables tables;Values life{1,2,3,4};
 const int types[]={92,93,94,97,98,99,90,89,88,87,86,91,95,96,114,115};
 for(unsigned rank=1;rank<=4;++rank)for(int type:types){
  Metadata m;m.itemGrade=Grade::S;m.options={{Family::Excellent,4,static_cast<Grade>(rank)}};
  auto v=tables.Resolve(m,16,7,type,777,true,life);assert(v.value>0 && v.value!=777);
  assert(tables.Resolve(m,16,7,type,777,false,life).value==777);
  assert(tables.Resolve(m,0,7,type,777,true,life).value==777);
  m.itemGrade=Grade::C;assert(tables.Resolve(m,16,7,type,777,true,life).value==v.value);
 }
 Metadata m;m.options={{Family::Excellent,4,Grade::S}};
 assert(tables.Resolve(m,16,7,90,4,true,life).kind==MaximumSD);assert(tables.Resolve(m,16,7,90,4,true,life).value==5);
 assert(tables.Resolve(m,16,7,98,8,true,life).kind==LifeOnKillDivisor);assert(tables.Resolve(m,16,7,98,8,true,life).value==6);
 assert(tables.Resolve(m,16,7,97,7,true,life).value==40);assert(tables.Resolve(m,16,7,93,20,true,life).value==15);
 assert(tables.Resolve(m,16,7,91,4,true,life).value==4);
 m.options.clear();assert(tables.Resolve(m,16,7,90,4,true,life).kind==90);
 m.options={{Family::Ancient,4,Grade::S}};assert(tables.Resolve(m,16,7,90,4,true,life).kind==90);
 m.options={{Family::Excellent,4,Grade::S}};tables.rows[1]={0,1,2,3};assert(tables.Resolve(m,16,7,93,20,true,life).value==20);
 assert(Recovery(1000,0,1.0/6)==166);assert(Recovery(1000,0,2.0/6)==333);assert(Recovery(1000,12,1.0/6)==286);
 assert(Recovery(1000,12,0,10)==130);assert(Recovery(0,0,0)==0);assert(Recovery(1e12,100,1)==2147483647);
 puts("PASS: all per-option grades, item/additional independence, SD conversion, rational recovery, no stacking on recalculation policy, disabled/legacy/Ancient isolation and invalid tables.");}
