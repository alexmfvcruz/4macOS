#include "../legacy_string.hpp"
#include "../../packet/wire.hpp"
#include <cassert>
#include <iostream>
#include <map>
#include <fstream>
using namespace fourstory::server;
int main() {
    LegacyString name("Character");assert(name.GetLength()==9 && !name.IsEmpty());
    assert(name.Left(4)==LegacyString("Char") && name.Right(3)==LegacyString("ter"));
    assert(name.Mid(4,2)==LegacyString("ac") && name.Mid(999).IsEmpty());
    assert(name.Find('a')==2 && name.ReverseFind('a')==4 && name.Find("missing")==-1);
    assert(!name.CompareNoCase(LegacyString("CHARACTER")));name.MakeUpper();
    assert(name==LegacyString("CHARACTER"));
    std::map<LegacyString,unsigned> names;names.emplace(name,123);
    assert(names.at(LegacyString("CHARACTER"))==123);
    const std::string encoded("\x81\x82\x83",3);LegacyString raw(encoded);
    story::Writer writer(1);writer.string_bytes(raw.bytes());auto packet=writer.finish();
    story::Reader reader(packet);assert(reader.string_bytes()==encoded);
    assert(raw.Left(2).GetLength()==2 && raw.GetLength()==3);
    raw.MakeUpper();assert(raw.bytes()==encoded);name.Empty();assert(name.IsEmpty());
    LegacyString accented(std::string(1,char(0xe9))),accented_upper(accented);
    accented_upper.MakeUpper();assert(accented_upper.bytes()==std::string(1,char(0xc9)));
    assert(accented.CompareNoCase(accented_upper)!=0); // Windows default CRT C locale
    assert(LegacyString("A").CompareNoCase(LegacyString("["))>0);
    std::ifstream fixture("research/fixtures/windows-string-casing.txt");
    std::string locale,upper_hex,equality;assert(std::getline(fixture,locale) && locale=="CRT=C");
    assert(std::getline(fixture,upper_hex) && upper_hex.size()==512);
    assert(std::getline(fixture,equality) && equality.size()==256);
    for(unsigned i=1;i<256;++i) {
        LegacyString original(std::string(1,static_cast<char>(i))),upper(original);upper.MakeUpper();
        const auto expected_byte=std::stoul(upper_hex.substr(i*2,2),nullptr,16);
        assert(static_cast<unsigned char>(upper.bytes()[0])==expected_byte);
        assert((original.CompareNoCase(upper)==0)==(equality[i]=='1'));
    }
    bool refused=false;
    name.Format("%s:%04X:%d:%ld:%I64u:%.2f%%",raw,0xab,0xffffffffu,0xffffffffu,
        std::uint64_t(0x100000000),1.25);
    assert(name.bytes()==encoded+":00AB:-1:-1:4294967296:1.25%");
    name=LegacyString(" \tvalue\r\n");name.Trim();assert(name==LegacyString("value"));
    refused=false;try{name.Format("%s",123);}catch(const std::invalid_argument&){refused=true;}
    assert(refused && name==LegacyString("value"));
    refused=false;try{name.Format("%n",123);}catch(const std::invalid_argument&){refused=true;}
    assert(refused);
    int position=0;LegacyString tokens(";;one;two;;");
    assert(tokens.Tokenize(";",position)=="one" && position==6);
    assert(tokens.Tokenize(";",position)=="two" && position==10);
    assert(tokens.Tokenize(";",position).IsEmpty() && position==-1);
    assert(tokens.Tokenize(";",position).IsEmpty() && position==-1);
    position=0;assert(LegacyString("tail").Tokenize(";",position)=="tail" && position==5);
    assert(LegacyString("tail").Tokenize(";",position).IsEmpty() && position==-1);
    std::cout<<"PASS legacy byte string slicing/search, ASCII name keys and unchanged packet encoding\n";
}
