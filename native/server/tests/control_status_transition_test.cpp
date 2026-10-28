#include "../control_status_transition.hpp"
#include "../control_service_request.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server;
int main() {
    unsigned denials=0;ControlManagerAuthority manager([&]{++denials;});
    assert(!manager.authenticated() && !manager.check(MANAGER_SERVICE));
    assert(!manager.authenticated_login(0));
    assert(manager.authenticated_login(MANAGER_SERVICE));
    assert(manager.check(MANAGER_SERVICE) && !manager.check(MANAGER_ALL) && denials==1);
    manager.revoke();assert(!manager.check(MANAGER_SERVICE) && denials==1);
    assert(manager.authenticated_login(MANAGER_ALL) && manager.check(MANAGER_SERVICE));
    assert(!manager.authenticated_login(MANAGER_COUNT) && !manager.authenticated());
    ControlServiceStatus state;state.m_dwID=0x010203;state.type.m_bType=2;
    unsigned ticks=0,logs=0;std::vector<std::unique_ptr<CPacket>> messages;
    auto tick=[&]{++ticks;return DWORD{123};};
    auto log=[&](const char*,const CString&){++logs;};
    auto batch=[&](std::unique_ptr<CPacket> packet){messages.push_back(std::move(packet));};
    original_control_status_transition(state,4,tick,log,batch);
    assert(state.m_bManagerControl==1 && messages.size()==1 && ticks==0);
    original_control_status_transition(state,4,tick,log,batch);assert(messages.size()==1);
    original_control_status_transition(state,1,tick,log,batch);
    assert(state.m_dwStopCount==1 && state.m_dwAutoStartTick==123 && ticks==1 && logs==1);
    original_control_status_transition(state,4,tick,log,batch);
    assert(messages.size()==3);
    for(std::size_t i=0;i<messages.size();++i) {
        DWORD id,status;BYTE sms,type;
        assert(messages[i]->GetID()==CT_SERVICECHANGE_REQ);
        *messages[i]>>id>>status>>sms>>type;
        assert(id==0x010203 && type==2 && status==(i==1?1u:4u) && sms==i);
    }
    state.m_dwStatus=1;state.m_dwAutoStartTick=100;state.m_bManagerControl=TRUE;
    DWORD now=60100;unsigned starts=0;
    auto clock=[&]{return now;};
    auto start=[&](DWORD id){assert(id==state.m_dwID);++starts;return false;};
    original_control_auto_start(state,1,TRUE,clock,start,log);assert(starts==0);
    ++now;original_control_auto_start(state,1,TRUE,clock,start,log);
    assert(starts==1 && state.m_dwAutoStartTick==now); // failed starts still consume original retry interval
    now+=60001;
    original_control_auto_start(state,1,FALSE,clock,start,log);assert(starts==1);
    state.m_bManagerControl=FALSE;
    original_control_auto_start(state,1,TRUE,clock,start,log);assert(starts==1);
    state.m_bManagerControl=TRUE;
    original_control_auto_start(state,4,TRUE,clock,start,log);assert(starts==1);
    original_control_auto_start(state,1,TRUE,clock,start,log);assert(starts==2);
    messages.clear();state.m_dwStatus=static_cast<DWORD>(-1);
    original_control_reconnect(state,4,[]{return 456;},log,batch);
    assert(messages.size()==1 && state.m_nLatestStop==0);
    DWORD reconnect_id=0;assert(messages[0]->GetID()==CT_NEWCONNECT_REQ);
    *messages[0]>>reconnect_id;assert(reconnect_id==state.m_dwID);
    messages.clear();state.m_dwStatus=1;
    original_control_reconnect(state,4,[]{return 456;},log,batch);
    assert(messages.size()==1 && state.m_nLatestStop==456);
    messages.clear();state.type.m_bType=SVRGRP_LOG;
    original_control_reconnect(state,4,[]{return 789;},log,batch);
    assert(messages.empty() && state.m_nLatestStop==789);
    ControlServiceStatus world,peer,other;
    world.m_dwID=1;world.m_dwStatus=4;world.type.m_bType=SVRGRP_WORLDSVR;world.group.m_bGroupID=1;
    peer.m_dwID=2;peer.group.m_bGroupID=1;peer.m_bManagerControl=TRUE;
    other.m_dwID=3;other.group.m_bGroupID=2;other.m_bManagerControl=TRUE;
    std::map<DWORD,ControlServiceStatus*> services{{1,&world},{2,&peer},{3,&other}};
    unsigned commands=0,acks=0;
    ControlServiceManagement control({{1,0}},[&](const char* action,std::size_t index){
        assert(std::string(action)=="stop" && index==0);++commands;return true;
    });
    auto authority=[](BYTE value){assert(value==MANAGER_SERVICE);return true;};
    auto ack=[&](BYTE value){assert(value==TRUE);++acks;};
    CPacket empty;
    assert(original_control_service_request(empty,false,authority,ack,services,control,clock,log)==EC_NOERROR);
    assert(original_control_service_request(empty,true,[](BYTE){return false;},ack,services,control,clock,log)==EC_NOERROR);
    assert(commands==0 && acks==0);
    CPacket request;request.SetID(CT_SERVICECONTROL_REQ);request<<DWORD{1}<<BYTE{0};
    Session::Packet wire(request.GetBuffer(),request.GetBuffer()+request.GetSize());
    auto incoming=received_packet(wire);
    assert(original_control_service_request(*incoming,true,authority,ack,services,control,clock,log)==EC_NOERROR);
    assert(commands==1 && acks==1 && world.m_bManagerControl==FALSE && peer.m_bManagerControl==FALSE &&
        other.m_bManagerControl==TRUE && peer.m_dwAutoStartTick==now);
    state.type.m_bType=2;state.m_dwStatus=4;
    original_control_reconnect(state,4,[]{return 999;},log,batch);
    assert(messages.empty() && state.m_nLatestStop==789);
    std::cout<<"PASS original Control status transitions, stop counters and service-change packet fields\n";
}
