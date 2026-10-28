// Include the actual formatter, avoiding a second implementation of its format.
#define main c1_dump_main
#include "dump.cpp"
#undef main
#include <cassert>
int main(int argc,char** argv){
    assert(argc==2);
    const char bytes[]={'\\','x','4','1','\t','\n','\r','\0',char(0x80)};
    assert(escaped(bytes,sizeof bytes)=="\\x5Cx41\\x09\\x0A\\x0D\\x00\\x80");
    output.open(argv[1],ios::binary|ios::trunc);assert(output);
    value("test","0","zero",0.f);value("test","0","negative-zero",-0.f);
    value("test","0","signed",-2);value("test","0","unsigned",uint32_t(0xffffffff));
    output.close();ifstream in(argv[1],ios::binary);string all((istreambuf_iterator<char>(in)),{});
    assert(all=="test\t0\tzero\t00000000\ntest\t0\tnegative-zero\t80000000\ntest\t0\tsigned\t-2\ntest\t0\tunsigned\t4294967295\n");
    cout<<"PASS shared dump escaping, signed zero, integer widths and LF output\n";
}
