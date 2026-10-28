#include "../role_lifecycle.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server;
int main() {
    bool rejected=false;try{RoleLifecycle incomplete(Service::map,{});}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    for(auto role:{Service::control,Service::login,Service::world,Service::map}) {
        std::vector<char> events;
        RoleLifecycle::Callbacks callbacks;
        callbacks.init_database=[&]{events.push_back('D');return 0u;};
        callbacks.load_data=[&]{events.push_back('L');return 0u;};
        callbacks.install_workers=[&](Runtime& runtime){assert(runtime.port()==0);events.push_back('W');};
        callbacks.init_environment=[&](Runtime& runtime){assert(runtime.port()!=0);events.push_back('E');};
        callbacks.clear_login_users=[&]{events.push_back('C');};
        callbacks.tournament_update_if_needed=[&]{events.push_back('T');};
        auto save=[&](Runtime& runtime){events.push_back('S');assert(runtime.cleanup_work(Lane::database,[&](const Runtime&){events.push_back('P');}));};
        callbacks.save_characters=save;callbacks.save_pvp=save;
        callbacks.update_data=[&]{events.push_back('U');};callbacks.close_database=[&]{events.push_back('X');};
        Runtime::Hooks transport;transport.activated=[](Runtime& runtime){assert(runtime.port()!=0);runtime.request_stop();};
        Config config;config.service=role;config.listen={"127.0.0.1",0};
        Config mismatched=config;
        mismatched.service=role==Service::control?Service::login:Service::control;
        Runtime wrong(mismatched,RoleLifecycle(role,callbacks).bind(transport));
        rejected=false;try{wrong.run();}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected && events.empty() && wrong.port()==0 && wrong.finished());
        Runtime runtime(config,RoleLifecycle(role,callbacks).bind(transport));runtime.run();
        std::string expected="DLW";
        if(role==Service::map) expected+="E";
        if(role==Service::login) expected+="C";
        if(role==Service::world) expected+="T";
        if(role==Service::world || role==Service::map) expected+="SP";
        expected+="UX";assert(std::string(events.begin(),events.end())==expected);
        events.clear();callbacks.init_database=[&]{events.push_back('D');return 77u;};
        Runtime failed(config,RoleLifecycle(role,callbacks).bind());
        rejected=false;try{failed.run();}catch(const RoleStartupError& error){assert(error.code()==77);rejected=true;}
        assert(rejected && failed.port()==0 && events==std::vector<char>({'D','U','X'}));
        events.clear();callbacks.init_database=[&]{events.push_back('D');return 0u;};
        callbacks.load_data=[&]{events.push_back('L');return 78u;};
        callbacks.update_data=[&]{events.push_back('U');throw std::runtime_error("cleanup error");};
        Runtime load_failed(config,RoleLifecycle(role,callbacks).bind());
        rejected=false;try{load_failed.run();}catch(const RoleStartupError& error){assert(error.code()==78);rejected=true;}
        assert(rejected && events==std::vector<char>({'D','L','U','X'}));
        if(role==Service::map) {
            events.clear();bool ran=false;
            callbacks.load_data=[&]{events.push_back('L');return 0u;};
            callbacks.update_data=[&]{events.push_back('U');};
            callbacks.init_environment=[&](Runtime& owner){
                assert(owner.port()!=0);events.push_back('E');
                assert(owner.work([&](const Runtime&){ran=true;}));
                throw std::runtime_error("environment setup failed");
            };
            Runtime environment_failed(config,RoleLifecycle(role,callbacks).bind());
            rejected=false;try{environment_failed.run();}catch(const std::runtime_error&){rejected=true;}
            assert(rejected && !ran && environment_failed.port()==0 &&
                   events==std::vector<char>({'D','L','W','E','U','X'}));
        }
    }
    std::cout<<"PASS four-role lifecycle order, pre-activation setup, save-before-close and original startup errors\n";
}
