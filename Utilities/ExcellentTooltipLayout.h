#pragma once
#include <cstddef>
#include <cstdio>
#include <cstring>
namespace ExcellentTooltipLayout {
struct Details {
    struct Entry {int row;const char* note;};
    Entry entries[6]{};
    unsigned size=0;
    void Add(int row,const char* note) {
        if(size>=6 || row<0 || !note || !*note)return;
        if(size && row<=entries[size-1].row)return;
        entries[size++]={row,note};
    }
    // Called only AFTER all native Ancient/socket/period rows are complete.
    // Preserve every native row and the reserved Gear Score/Harmony budget.
    template<std::size_t Rows,std::size_t Columns,class Color,class Bold>
    bool Apply(char (&text)[Rows][Columns],Color (&colors)[Rows],Bold (&bold)[Rows],int& count,Color noteColor,unsigned reserve) const {
        if(count<0 || static_cast<std::size_t>(count)>Rows || reserve>Rows ||
           size>Rows-static_cast<std::size_t>(count) || reserve>Rows-count-size)return false;
        for(unsigned n=0;n<size;++n)if(entries[n].row>=count || std::strlen(entries[n].note)>=Columns)return false;
        for(unsigned n=size;n>0;--n) {
            const auto& entry=entries[n-1];const int at=entry.row+1;
            const auto remaining=static_cast<std::size_t>(count-at);
            std::memmove(text+at+1,text+at,remaining*sizeof(text[0]));
            std::memmove(colors+at+1,colors+at,remaining*sizeof(colors[0]));
            std::memmove(bold+at+1,bold+at,remaining*sizeof(bold[0]));
            std::snprintf(text[at],Columns,"%s",entry.note);
            colors[at]=noteColor;bold[at]=false;++count;
        }
        return true;
    }
};
}
