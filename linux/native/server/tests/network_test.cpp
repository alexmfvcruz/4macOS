#include "../network.hpp"
#include "../received_packet.hpp"
#include "../../packet/wire.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server;
int main() {
    Network::Id accepted=0,connected=0;unsigned received=0,closed=0;
    story::Writer writer(42);writer.string_bytes("native TCP");auto packet=writer.finish();
    Network* server_ptr=nullptr;
    Network server({[&](auto id,bool incoming){assert(incoming);accepted=id;},
        [&](auto id,Session::Packet bytes){
            assert(bytes==packet);
            auto original=received_packet(bytes);CString text;*original>>text;
            assert(original->GetID()==42 && text.bytes()=="native TCP" && original->IsEOF());
            ++received;assert(server_ptr->send(id,std::move(bytes)));
        },
        [&](auto){++closed;}});
    server_ptr=&server;
    Network client({[&](auto id,bool incoming){assert(!incoming);connected=id;},
        [&](auto,Session::Packet bytes){assert(bytes==packet);++received;},[&](auto){++closed;}});
    auto port=server.listen({"127.0.0.1",0});auto original=client.connect({"127.0.0.1",port});
    for(unsigned i=0;(!accepted || !connected) && i<100;++i){server.step(1);client.step(1);}
    assert(accepted && connected==original);assert(client.send(original,packet));
    for(unsigned i=0;received<2 && i<100;++i){client.step(1);server.step(1);}
    assert(received==2);client.drain();
    for(unsigned i=0;server.connections() && i<100;++i) server.step(1);
    assert(!server.connections() && !client.connections() && closed==2);
    assert(!client.send(original,packet));
    connected=0;accepted=0;auto replacement=client.connect({"127.0.0.1",port});
    for(unsigned i=0;(!accepted || !connected) && i<100;++i){server.step(1);client.step(1);}
    assert(replacement!=original && connected==replacement);
    assert(!client.send(original,packet));assert(client.send(replacement,packet));
    for(unsigned i=0;received<4 && i<100;++i){client.step(1);server.step(1);}
    assert(received==4);client.close_all();server.close_all();assert(closed==4);
    // A refused nonblocking connect must produce close, never a false open.
    unsigned false_opens=0,failed=0;
    Network refused({[&](auto,bool){++false_opens;},{},[&](auto){++failed;}});
    try {refused.connect({"127.0.0.1",port});}
    catch(const std::system_error&) {++failed;}
    for(unsigned i=0;refused.connections() && i<100;++i) refused.step(1);
    assert(false_opens==0 && failed==1 && !refused.connections());
    std::cout<<"PASS IPv4 accept/connect, packet round trip, drain/EOF, reconnect IDs and refused connect\n";
}
