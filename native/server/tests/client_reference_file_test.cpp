#include "../client_reference_file.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace fourstory::server;
int main() {
    char name[]="/tmp/4story-reference-XXXXXX";auto fd=mkstemp(name);assert(fd>=0);
    const unsigned char bytes[]={1,2,3,4,5,6,7,8,9};
    assert(write(fd,bytes,sizeof(bytes))==sizeof(bytes));::close(fd);
    ClientReferenceFile m_clientReference;
    const char* client_reference_path=name;std::uint64_t m_dlCheckFile=0;
#define FOURSTORY_NATIVE_SERVER
#include "../../../source/4Story 5.0 Source Files/Server/TLoginSvr/ClientReference.inc"
    assert(m_dlCheckFile==8);
    ClientReferenceFile reference;assert(reference.open(name));
    assert(reference.available() && reference.checksum()==8); // retained bytes, not zero-padded tail
    fd=::open(name,O_WRONLY|O_TRUNC);assert(fd>=0);assert(write(fd,bytes,3)==3);::close(fd);
    assert(reference.open(name) && reference.checksum()==0x030201);
    fd=::open(name,O_WRONLY|O_TRUNC);assert(fd>=0);::close(fd);
    assert(reference.open(name) && reference.checksum()==0);
    reference.close();reference.close();assert(!reference.available());
    unlink(name);assert(!reference.open(name) && reference.checksum()==0);
    assert(!reference.open("/tmp"));
    std::cout<<"PASS Login client reference checksum, retained tail bytes, empty/missing files and readonly ownership\n";
}
