#include "../DataServer/DataServer/stdafx.h"
#include "../DataServer/DataServer/IndependentTierStore.h"
#include "../DataServer/DataServer/Util.h"
#include <cassert>
#include <cstdio>
void LogAdd(eLogColor,char* text,...) { va_list a;va_start(a,text);vfprintf(stderr,text,a);fputc('\n',stderr);va_end(a); }
int main() {
 char dsn[]="MuOnlineBase",user[]="",password[]="";
 CQueryManager query;
 if(!query.Connect(dsn,user,password)){puts("ODBC connection failed");return 2;}
 assert(query.ExecQuery("BEGIN TRANSACTION"));query.Close();
 using namespace IndependentTierStore;using namespace IndependentItemTiers;
 const std::uint32_t serial=4294967280u;Record before;
 if(Read(query,serial,before)!=Status::Missing){query.ExecQuery("ROLLBACK TRANSACTION");query.Close();puts("Reserved test serial already exists; no write performed");return 3;}
 Metadata metadata;metadata.itemGrade=Grade::C;metadata.options={{Family::Excellent,5,Grade::S},{Family::Ancient,0,Grade::A}};
 assert(Create(query,serial,4096,metadata)==Status::Created);
 Record record;assert(Read(query,serial,record)==Status::Found);assert(record.originalItemId==4096 && record.metadata.itemGrade==Grade::C && record.metadata.Get(Family::Excellent,5)==Grade::S && record.metadata.Get(Family::Ancient,0)==Grade::A);
 assert(Create(query,serial,4096,metadata)==Status::Existing);
 Metadata changed=metadata;changed.itemGrade=Grade::S;assert(Create(query,serial,4096,changed)==Status::Conflict);
 assert(query.ExecQuery("ROLLBACK TRANSACTION"));query.Close();assert(Read(query,serial,record)==Status::Missing);
 puts("PASS: actual x86 DataServer ODBC adapter, SQL store, binary decoding, independent C/S/A grades, immutable retry/conflict and transaction rollback.");
}
