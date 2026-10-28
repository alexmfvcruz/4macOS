#include <TCMLParser.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
int main(int argc,char** argv){
    assert(argc==2);std::ofstream script(argv[1],std::ios::binary);assert(script);
    script<<"<TCML><HEAD><DEFINE ROOT=1></HEAD><FRAME ID=\"ROOT\">";
    for(int i=0;i<300;++i)script<<"<COMPONENT>";
    for(int i=0;i<300;++i)script<<"</COMPONENT>";
    script<<"</FRAME></TCML>";script.close();
    std::vector<char> path(argv[1],argv[1]+std::strlen(argv[1])+1);TCMLParser parser;
    assert(parser.Parse(path.data())==0);assert(parser.m_Comps.size()==1);
    auto root=parser.m_Comps.begin()->second;int depth=0;for(auto p=root->child;p;p=p->child)++depth;assert(depth==300);
    std::cout<<"PASS TCML 300 nested components (generated-parser stack growth)\n";
}
