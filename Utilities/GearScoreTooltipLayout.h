#pragma once
#include <cstdio>
#include <cstring>
#include <cstdint>
namespace GearScoreTooltipLayout {
// Insert after native options are complete; do not consume their row budget.
template<std::size_t Rows,std::size_t Columns,class Color,class Bold>
bool Insert(char (&text)[Rows][Columns],Color (&colors)[Rows],Bold (&bold)[Rows],int& count,int after,
    unsigned base,unsigned grade,const char* baseFormat,const char* gradeFormat,const char* totalFormat,
    const char* compactFormat,Color baseColor,Color gradeColor,Color totalColor) {
    if(count<0 || static_cast<std::size_t>(count)>Rows || after<0 || after>count || base>1000000 || grade>1000000)return false;
    char lines[3][Columns]{};
    const unsigned values[]={base,grade,base+grade};
    const char* formats[]={baseFormat,gradeFormat,totalFormat};
    const Color palette[]={baseColor,gradeColor,totalColor};
    for(unsigned n=0;n<3;++n) {
        const int size=std::snprintf(lines[n],Columns,formats[n],values[n]);
        if(size<0 || static_cast<std::size_t>(size)>=Columns)return false;
    }
    if(Rows-static_cast<std::size_t>(count)>=3) {
        const auto move=static_cast<std::size_t>(count-after);
        std::memmove(text+after+3,text+after,move*sizeof(text[0]));
        std::memmove(colors+after+3,colors+after,move*sizeof(colors[0]));
        std::memmove(bold+after+3,bold+after,move*sizeof(bold[0]));
        for(unsigned n=0;n<3;++n) {
            std::memcpy(text[after+n],lines[n],Columns);colors[after+n]=palette[n];bold[after+n]=true;
        }
        count+=3;return true;
    }
    // Dense Ancient/socket tooltip: retain every option and use one summary row.
    char compact[Columns]{};
    const int size=std::snprintf(compact,Columns,compactFormat,base,grade,base+grade);
    if(size<0 || static_cast<std::size_t>(size)>=Columns)return false;
    int target=count;
    if(static_cast<std::size_t>(count)==Rows) {
        target=-1;
        for(int row=after;row<count;++row)if(!text[row][0] || std::strcmp(text[row],"\n")==0){target=row;break;}
        if(target<0)return false;
    } else ++count;
    std::memcpy(text[target],compact,Columns);colors[target]=totalColor;bold[target]=true;return true;
}
}
