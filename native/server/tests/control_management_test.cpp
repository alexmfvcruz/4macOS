#include "../original_control_management.hpp"
#include "../supervisor_commands.hpp"
#include "../control_status_transition.hpp"
#include "../control_management_worker.hpp"
#include <cassert>
#include <thread>
#include <cstdlib>
#include <csignal>
#include <iostream>
using namespace fourstory::server;
int main(int argc,char** argv) {
    assert(argc==3);
    // The host already ignores SIGPIPE; match that policy for this pipe consumer.
    std::signal(SIGPIPE,SIG_IGN);
    SupervisorCommands backend(std::atoi(argv[1]),std::atoi(argv[2]));
    ControlServiceManagement management({{0x010203,0}},
        [&](const char* action,std::size_t index){return backend(action,index);},
        [&](std::size_t index){return backend.status(index);});
    assert(management.status(0x999999)==static_cast<DWORD>(-1));
    assert(management.status(0x010203)==4);
    auto managed=std::make_shared<ControlServiceManagement>(management);
    auto observed=std::make_shared<ControlServiceStatus>();observed->m_dwID=0x010203;observed->type.m_bType=2;
    Config config;config.service=Service::control;config.listen={"127.0.0.1",0};
    std::vector<WORD> worker_notifications;
    Runtime::Hooks hooks;
    hooks.prepare=[&](Runtime& runtime) {
        runtime.configure_loop(Lane::management,control_management_worker(runtime,managed,{observed},FALSE,
            [](const char*,const CString&){},[&runtime,&worker_notifications](CPacket& packet) {
                worker_notifications.push_back(packet.GetID());
                if(worker_notifications.size()==2) runtime.request_stop();
            }));
    };
    const auto runtime_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    hooks.tick=[&](Runtime& runtime){if(std::chrono::steady_clock::now()>runtime_deadline) runtime.request_stop();};
    Runtime runtime(config,hooks);runtime.run();
    assert(worker_notifications==std::vector<WORD>({CT_NEWCONNECT_REQ,CT_SERVICECHANGE_REQ}) && runtime.finished());
    ControlServiceStatus service;service.m_dwID=0x010203;service.type.m_bType=2;
    std::vector<WORD> notifications;
    original_control_poll_status(service,management,FALSE,[]{return DWORD{0};},[]{return 123;},
        [](const char*,const CString&){},[&](std::unique_ptr<CPacket> packet){notifications.push_back(packet->GetID());});
    assert(service.m_dwStatus==4 && notifications==std::vector<WORD>({CT_NEWCONNECT_REQ,CT_SERVICECHANGE_REQ}));
    for(const std::string response:{
        "CONTROL {\"ok\":true,\"service\":0,\"state\":10}\n",
        "CONTROL {\"ok\":true,\"service\":1,\"state\":4}\n"}) {
        int requests[2],responses[2];assert(::pipe(requests)==0 && ::pipe(responses)==0);
        assert(::write(responses[1],response.data(),response.size())==static_cast<ssize_t>(response.size()));
        SupervisorCommands malformed(requests[1],responses[0]);
        assert(!malformed.status(0));
        assert(!malformed("start",0)); // malformed response permanently invalidates this stream
        for(auto fd:{requests[0],requests[1],responses[0],responses[1]}) ::close(fd);
    }
    assert(original_control_start(management,0x999999)==FALSE);
    assert(original_control_start(management,0x010203)==FALSE);
    assert(original_control_stop(management,0x010203)==TRUE);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(management.status(0x010203)!=1) {
        assert(std::chrono::steady_clock::now()<deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    while(!original_control_start(management,0x010203)) {
        assert(std::chrono::steady_clock::now()<deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    assert(management.status(0x010203)==4);
    std::cout<<"PASS original Control start/stop fragments through real supervisor pipes\n";
}
