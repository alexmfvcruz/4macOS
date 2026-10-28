// The same source is built with original MFC/D3DX on Windows and C1 on macOS.
// No platform-specific source branches or native compatibility headers.
#include <afx.h>
#include <d3dx9.h>
#include <vector>
#include <map>
#include <set>
using namespace std;
#include <NetCode.h>
#include <TChartType.h>
#include <TChart.h>
#include <TCMLParser.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <algorithm>

namespace {
std::ofstream output;
string escaped(const char* p, size_t n) {
    static const char hex[]="0123456789ABCDEF";
    string s;
    for(size_t i=0;i<n;++i){unsigned char c=static_cast<unsigned char>(p[i]);
        if(c>=32&&c<=126&&c!='\\')s+=char(c);
        else {s+="\\x";s+=hex[c>>4];s+=hex[c&15];}}
    return s;
}
void record(const string& dataset,const string& key,const string& field,const string& value) {
    output<<dataset<<'\t'<<key<<'\t'<<field<<'\t'<<value<<'\n';
    if(!output)throw runtime_error("dump output write failed");
}
template<class T> typename enable_if<is_integral<T>::value,void>::type
value(const string& d,const string& k,const string& f,T v){
    record(d,k,f,is_signed<T>::value?to_string(static_cast<long long>(v)):to_string(static_cast<unsigned long long>(v)));
}
void value(const string& d,const string& k,const string& f,float v){
    uint32_t bits;memcpy(&bits,&v,4);ostringstream s;s<<uppercase<<hex<<setw(8)<<setfill('0')<<bits;record(d,k,f,s.str());
}
void value(const string& d,const string& k,const string& f,const CString& v){record(d,k,f,escaped(LPCSTR(v),static_cast<size_t>(v.GetLength())));}
void value(const string& d,const string& k,const string& f,const D3DXVECTOR3& v){value(d,k,f+".x",v.x);value(d,k,f+".y",v.y);value(d,k,f+".z",v.z);}
template<class T,size_t N> void value(const string& d,const string& k,const string& f,const T(&v)[N]){for(size_t i=0;i<N;++i)value(d,k,f+"["+to_string(i)+"]",v[i]);}
#define FIELD(name) value(d,k,#name,p.name)

void object(const string& d,const string& k,const TITEM& p){
    FIELD(m_wItemID);
    FIELD(m_bType);
    FIELD(m_bKind);
    FIELD(m_wAttrID);
    FIELD(m_strNAME);
    FIELD(m_wUseValue);
    FIELD(m_dwSlotID);
    FIELD(m_dwClassID);
    FIELD(m_bPrmSlotID);
    FIELD(m_bSubSlotID);
    FIELD(m_bLevel);
    FIELD(m_bCanRepair);
    FIELD(m_dwDuraMax);
    FIELD(m_bRefineMax);
    FIELD(m_fPriceRate);
    FIELD(m_dwPrice);
    FIELD(m_bMinRange);
    FIELD(m_bMaxRange);
    FIELD(m_bStack);
    FIELD(m_bSlotCount);
    FIELD(m_bCanGamble);
    FIELD(m_bGambleProb);
    FIELD(m_bDestoryProb);
    FIELD(m_bCanGrade);
    FIELD(m_bCanMagic);
    FIELD(m_bCanRare);
    FIELD(m_wDelayGroupID);
    FIELD(m_dwDelay);
    FIELD(m_bCanTrade);
    FIELD(m_bIsSpecial);
    FIELD(m_wUseTime);
    FIELD(m_bUseType);
    FIELD(m_bWeaponID);
    FIELD(m_fShotSpeed);
    FIELD(m_fGravity);
    FIELD(m_dwInfoID);
    FIELD(m_bSkillItemType);
    FIELD(m_wVisual);
    FIELD(m_wGradeSFX);
    FIELD(m_wOptionSFX);
    FIELD(m_bCanWrap);
    FIELD(m_dwAuctionCode);
    FIELD(m_wUseBP);
    FIELD(m_bCanColor);
}

void object(const string& d,const string& k,const TSKILL& p){
    FIELD(m_wSkillID);
    FIELD(m_strNAME);
    FIELD(m_wActSkillID);
    FIELD(m_wDefSkillID);
    FIELD(m_wModeID);
    FIELD(m_dwClassID);
    FIELD(m_bKind);
    FIELD(m_dwWeaponID);
    FIELD(m_dwDuration);
    FIELD(m_dwDurationInc);
    FIELD(m_wUseHP);
    FIELD(m_wUseMP);
    FIELD(m_wIconID);
    FIELD(m_bLevel);
    FIELD(m_bLevelInc);
    FIELD(m_bMaxLevel);
    FIELD(m_fPrice);
    FIELD(m_wItemID);
    FIELD(m_wNeedItemID);
    FIELD(m_fMoveDist);
    FIELD(m_fAtkRange);
    FIELD(m_fBufRange);
    FIELD(m_fMinRange);
    FIELD(m_fMaxRange);
    FIELD(m_dwSpellTick);
    FIELD(m_bIsRide);
    FIELD(m_dwDelay);
    FIELD(m_nDelayInc);
    FIELD(m_dwGroupTick);
    FIELD(m_dwInterval);
    FIELD(m_wUseMedals);
    FIELD(m_bPositive);
    FIELD(m_bActive);
    FIELD(m_bDelayType);
    FIELD(m_bLoop);
    FIELD(m_bContinue);
    FIELD(m_bActionID);
    FIELD(m_dwSFX);
    FIELD(m_dwInfoID);
    FIELD(m_bTargetType);
    FIELD(m_bRangeType);
    FIELD(m_bShowIcon);
    FIELD(m_bPremIconID);
    FIELD(m_bShowTime);
    FIELD(m_bShowCritical);
    FIELD(m_bUseInHold);
    FIELD(m_bDuraSlot);
    FIELD(m_bHitInit);
    FIELD(m_bHitInc);
    FIELD(m_bCanCancel);
    FIELD(m_bStaticWhenDie);
}

void object(const string& d,const string& k,const TLIGHT& p){
    FIELD(m_vDIR);
    FIELD(m_fAmbientR);
    FIELD(m_fAmbientG);
    FIELD(m_fAmbientB);
    FIELD(m_fDiffuseR);
    FIELD(m_fDiffuseG);
    FIELD(m_fDiffuseB);
}

void object(const string& d,const string& k,const TFOG& p){
    FIELD(m_bType);
    FIELD(m_bRValue);
    FIELD(m_bGValue);
    FIELD(m_bBValue);
    FIELD(m_fRADIUS);
    FIELD(m_fRANGE);
    FIELD(m_fPosX);
    FIELD(m_fPosZ);
    FIELD(m_fDENSITY);
    FIELD(m_fSTART);
    FIELD(m_fEND);
}

void object(const string& d,const string& k,const TNPC& p){
    FIELD(m_dwQuestID);
    FIELD(m_dwHouseID);
    FIELD(m_dwExecID);
    FIELD(m_dwMaxHP);
    FIELD(m_dwID);
    FIELD(m_wItemID);
    FIELD(m_strTITLE);
    FIELD(m_strNAME);
    FIELD(m_bCollisionType);
    FIELD(m_bCanSelected);
    FIELD(m_bCountryID);
    FIELD(m_bDrawGhost);
    FIELD(m_bHouseMesh);
    FIELD(m_bDrawMark);
    FIELD(m_bDrawName);
    FIELD(m_bNPCType);
    FIELD(m_bClassID);
    FIELD(m_bLevel);
    FIELD(m_bLand);
    FIELD(m_bMode);
    FIELD(m_bCamp);
    FIELD(m_fScaleX);
    FIELD(m_fScaleY);
    FIELD(m_fScaleZ);
    FIELD(m_fSizeX);
    FIELD(m_fSizeY);
    FIELD(m_fSizeZ);
    FIELD(m_fPosX);
    FIELD(m_fPosY);
    FIELD(m_fPosZ);
    FIELD(m_fDIR);
}
#undef FIELD
string asset(const string& root,const char* name){return root+"/"+name;}
void raw(const string& root,const char* name){
    ifstream in(asset(root,name),ios::binary);if(!in)throw runtime_error(string("missing asset: ")+name);
    char bytes[32];uint64_t offset=0;
    while(in.read(bytes,sizeof bytes)||in.gcount()){
        record(string("asset.raw/")+name,to_string(offset),"bytes",escaped(bytes,static_cast<size_t>(in.gcount())));
        offset+=static_cast<uint64_t>(in.gcount());
    }
    if(!in.eof())throw runtime_error("asset read failed");
    value(string("asset.raw/")+name,"0","size",offset);
}
template<class Map> void objects(const string& name,const Map& map){
    value(name,"0","count",map.size());
    for(const auto& entry:map){if(!entry.second)throw runtime_error("null chart object");object(name,to_string(entry.first),*entry.second);}
}
void layout(const FRAMEDESC_SHAREDPTR& node,const string& root,const string& path){
    if(!node)return;
    const COMPINST& p=node->m_vCOMP;string d="TClientCmd",k=root+":"+path;
#define COMP(name) value(d,k,#name,p.name)
    COMP(m_dwID);COMP(m_bType);COMP(m_vMENU);COMP(m_vImageID);COMP(m_dwTooltipID);COMP(m_dwFontID);
    COMP(m_dwStyle);COMP(m_dwCOLOR);COMP(m_dwSND);COMP(m_nMargineH);COMP(m_nMargineV);
    COMP(m_nPosX);COMP(m_nPosY);COMP(m_nWidth);COMP(m_nHeight);COMP(m_bDisplay);COMP(m_bAlign);
    COMP(m_strTooltip);COMP(m_strText);
#undef COMP
    static_assert(sizeof(TSATR)==68,"original TSATR disk layout");
    // All union words were read from the file; memcpy avoids inactive-member reads.
    uint32_t attributes[17];memcpy(attributes,&p.m_vEX,sizeof attributes);value(d,k,"m_vEX.words",attributes);
    size_t index=0;
    for(auto child=node->m_pCHILD;child;child=child->m_pNEXT)layout(child,root,path+"/"+to_string(index++));
    value(d,k,"children",index);
}
void fonts(TCMLParser& parser){
    value("TClientFont","0","count",parser.m_Fonts.size());
    for(const auto& entry:parser.m_Fonts){const TCML_LOGFONT& p=*entry.second;string d="TClientFont",k=to_string(entry.first);
#define DUMP_FONT(name) value(d,k,#name,p.name)
        DUMP_FONT(tlfHeight);DUMP_FONT(tlfWidth);DUMP_FONT(tlfEscapement);DUMP_FONT(tlfOrientation);DUMP_FONT(tlfWeight);
        DUMP_FONT(tlfItalic);DUMP_FONT(tlfUnderline);DUMP_FONT(tlfStrikeOut);DUMP_FONT(tlfCharSet);DUMP_FONT(tlfOutPrecision);
        DUMP_FONT(tlfClipPrecision);DUMP_FONT(tlfQuality);DUMP_FONT(tlfPitchAndFamily);DUMP_FONT(tlfId);DUMP_FONT(tlfColor);
#undef DUMP_FONT
        // The complete 32-byte face-name payload, including trailing bytes.
        record(d,k,"tlfFaceName.bytes",escaped(p.tlfFaceName,sizeof p.tlfFaceName));
    }
}
void releaseCharts(){
    CTChart::ReleaseTITEMTEMP();CTChart::ReleaseTSKILLTEMP();CTChart::ReleaseTENV();
    CTChart::ReleaseTLIGHT();CTChart::ReleaseTFOG();CTChart::ReleaseTMAPINFO();CTChart::ReleaseTNPCGlobal();
}
}
int main(int argc,char** argv){
    if(argc!=3){cerr<<"usage: c1-dump ASSET_ROOT OUTPUT.tsv\n";return 2;}
    try {
        string root=argv[1];output.open(argv[2],ios::binary|ios::trunc);if(!output)throw runtime_error("cannot open dump output");
        const char* files[]={"Tcd/TString.tcd","Tcd/TItem.tcd","Tcd/TSkill.tcd","Tcd/TENV.tcd","Tcd/TLIGHT.tcd","Tcd/TFog.tcd","Tcd/TNODE.tcd","Tcd/TMAP.tcd","Tcd/TNPCGlobal.tcd","TClientCmd.tif"};
        // Load through the public library APIs. Emit only retained, initialized fields.
        if(!CTChart::InitTString(CString(asset(root,files[0]).c_str())))throw runtime_error("TString did not load");
        CTChart::InitTITEMTEMP(CString(asset(root,files[1]).c_str()));
        CTChart::InitTSKILLTEMP(CString(asset(root,files[2]).c_str()));
        CTChart::InitTENV(CString(asset(root,files[3]).c_str()));
        CTChart::InitTLIGHT(CString(asset(root,files[4]).c_str()));
        CTChart::InitTFOG(CString(asset(root,files[5]).c_str()));
        CTChart::InitTMAPINFO(CString(asset(root,files[6]).c_str()),CString(asset(root,files[7]).c_str()));
        CTChart::InitTNPCGlobal(CString(asset(root,files[8]).c_str()));
        TCMLParser parser;string filename=asset(root,files[9]);vector<char> path(filename.begin(),filename.end());path.push_back(0);parser.Load(path.data());
        if(parser.m_Frames.empty())throw runtime_error("layout has no frames");
        // Dataset order is alphabetical; maps sort numeric keys, lists retain source order.
        value("TClientCmd","0","count",parser.m_Frames.size());for(const auto& entry:parser.m_Frames)layout(entry.second,to_string(entry.first),"root");fonts(parser);
        value("TENV","0","count",CTChart::m_mapTENVINFO.size());
        for(const auto& entry:CTChart::m_mapTENVINFO){string k=to_string(entry.first);value("TENV",k,"count",entry.second->size());for(size_t i=0;i<entry.second->size();++i)value("TENV",k,"environment["+to_string(i)+"]",(*entry.second)[i]);}
        value("TFog","0","count",CTChart::m_mapTFOG.size());
        for(const auto& entry:CTChart::m_mapTFOG){string k=to_string(entry.first);const auto& fog=*entry.second;value("TFog",k,"global.present",fog.m_pTGLOBAL!=nullptr);if(fog.m_pTGLOBAL)object("TFog",k+":global",*fog.m_pTGLOBAL);value("TFog",k,"local.count",fog.m_vTLOCAL.size());for(size_t i=0;i<fog.m_vTLOCAL.size();++i)object("TFog",k+":local/"+to_string(i),*fog.m_vTLOCAL[i]);}
        objects("TItem",CTChart::m_mapTITEMTEMP);objects("TLIGHT",CTChart::m_mapTLIGHT);
        value("TMAP","0","count",CTChart::m_mapTMAPINFO.size());
        for(const auto& entry:CTChart::m_mapTMAPINFO){string k=to_string(entry.first);const TMAPINFO& m=*entry.second;value("TMAP",k,"m_dwMapID",m.m_dwMapID);value("TMAP",k,"m_bDungeon",m.m_bDungeon);value("TMAP",k,"m_bNPCCALL",m.m_bNPCCALL);value("TMAP",k,"nodes.count",m.m_mapTNODE.size());}
        for(const auto& entry:CTChart::m_mapTMAPINFO){string k=to_string(entry.first);for(const auto& node:entry.second->m_mapTNODE)value("TNODE.computed",k+":"+to_string(node.first),"position",*node.second);}
        objects("TNPCGlobal",CTChart::m_mapGlobalNPC);objects("TSkill",CTChart::m_mapTSKILLTEMP);
        value("TString","0","nation",CTChart::m_strNationCode);for(int i=0;i<TSTR_COUNT;++i)value("TString",to_string(i),"text",CTChart::m_vTSTRING[i]);
        // Supplemental raw inputs preserve reserved scalars, map transforms and
        // NPC IDs which the public APIs discard. The layout is fully decoded.
        vector<string> supplemental(files,files+9);sort(supplemental.begin(),supplemental.end());
        for(const string& f:supplemental)raw(root,f.c_str());
        releaseCharts();output.close();if(!output)throw runtime_error("dump output close failed");cout<<"PASS chart and layout dump\n";return 0;
    } catch(CException* e){e->Delete();cerr<<"MFC file/archive exception\n";return 1;}
      catch(const exception& e){cerr<<e.what()<<'\n';return 1;}
}
