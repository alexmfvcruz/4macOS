#include "../config.hpp"
#include <cassert>
#include <sstream>
#include <iostream>
using namespace fourstory::server;
const std::string base="service=login\nport=4816\nserver_id=1\ndb_dsn=TGLOBAL_GSP\ndb_user=audit\ndb_password_env=FOURSTORY_DB_PASSWORD\n";
void reject(const std::string& text) {
    std::istringstream input(text);bool failed=false;
    try { (void)read_config(input); } catch(const std::runtime_error&) { failed=true; }
    assert(failed);
}
int main() {
    std::istringstream input(base);auto c=read_config(input);
    assert(c.service==Service::login && c.listen.port==4816 && c.server_id==1);
    reject(base+"port=1\n");reject(base+"unexpected=sensitive-value\n");
    reject("service=login\nport=65536\nserver_id=1\ndb_dsn=x\ndb_user=x\ndb_password_env=X\n");
    reject("service=login\nport=1\nserver_id=256\ndb_dsn=x\ndb_user=x\ndb_password_env=X\n");
    reject(base+"listen_address=not-an-address\n");
    reject(base+"reconnect_initial_ms=200\nreconnect_max_ms=100\n");
    auto map=base;map.replace(map.find("login"),5,"map");reject(map);
    map+="group_id=255\nworld_address=127.0.0.1\nworld_port=3816\n";
    reject(map);map+="log_address=127.0.0.1\nlog_port=4819\n";
    std::istringstream map_input(map);assert(read_config(map_input).group_id==255);
    auto world=base;world.replace(world.find("login"),5,"world");reject(world);
    std::istringstream world_input(world+"group_id=1\n");assert(read_config(world_input).group_id==1);
    std::istringstream control_input("service=control\nport=3810\ndb_dsn=x\ndb_user=x\ndb_password_env=X\nauto_start=1\n");
    assert(read_config(control_input).auto_start);
    reject(base+"auto_start=2\n");
    reject(base+"dependency_loss=unknown\n");
    std::istringstream retry_input(base+"dependency_loss=retry\n");
    assert(read_config(retry_input).dependency_loss==DependencyLoss::retry);
    std::cout<<"PASS native typed config, widths, endpoints, strict keys and reconnect intervals\n";
}
