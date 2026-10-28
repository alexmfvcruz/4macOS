// Link the real TChart translation unit, not a duplicate formatting helper.
#include "StdAfx.h"
#include "TChart.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main(){
    const TSTRING id=TSTR_QUIT_TITLE;
    CString saved=CTChart::m_vTSTRING[id];
    CTChart::m_vTSTRING[id]="%s|%s|%d|%.2f|%%";
    const CString first("alpha");CString second("beta");
    CString result=CTChart::Format(id,first,second,7,1.25);
    assert(std::strcmp(result,"alpha|beta|7|1.25|%")==0);
    result=CTChart::Format(id,CString("temporary"),"literal",-2,.5f);
    assert(std::strcmp(result,"temporary|literal|-2|0.50|%")==0);
    CTChart::m_vTSTRING[id]="literal %%";
    assert(CTChart::Format(id)=="literal %");
    CTChart::m_vTSTRING[id]="%I64d|%s";
    assert(CTChart::Format(id,static_cast<long long>(1234567890123LL),CString("wide"))=="1234567890123|wide");
    assert(CTChart::Format(TSTR_COUNT,CString("unused")).IsEmpty());
    // The existing out-of-range custom override must still take precedence.
    assert(CTChart::CustomStrings[0][0]=="3000");
    CString custom=CTChart::CustomStrings[0][1];CTChart::CustomStrings[0][1]="custom %s %d";
    assert(CTChart::Format(static_cast<TSTRING>(3000),CString("override"),9)=="custom override 9");
    CTChart::CustomStrings[0][1]=custom;CTChart::m_vTSTRING[id]=saved;
    std::cout<<"PASS original CTChart::Format: CString, temporaries, literals, promotions, no args, invalid ID, custom override\n";
}
