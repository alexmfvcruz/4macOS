#include "../original_role_database.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server;
int main() {
    for(auto role:{Service::control,Service::login,Service::world,Service::map}) {
        Config config;config.service=role;config.listen={"127.0.0.1",0};
        config.db_dsn="4Story_Nonexistent_Test_DSN";config.db_user="";config.db_password_env="TEST_NAME_ONLY";
        unsigned reads=0;bool updated=false;
        Runtime::Hooks hooks;
        std::weak_ptr<LoginStartupData> retained_startup;
        std::weak_ptr<control_data::StartupData> retained_control;
        std::weak_ptr<world_data::WorldStartupData> retained_world;
        {
            OriginalRoleDatabase module(config,[&](const std::string& name){assert(name=="TEST_NAME_ONLY");++reads;return std::string{};});
            RoleLifecycle::Callbacks application;
            application.load_data=[] {assert(false);return 0u;};
            application.install_workers=[](Runtime&){assert(false);};
            application.init_environment=[](Runtime&){assert(false);};
            application.tournament_update_if_needed=[] {assert(false);};
            application.save_pvp=[](Runtime&){assert(false);};
            application.save_characters=[](Runtime&){assert(false);};
            application.update_data=[&]{updated=true;};
            if(role!=Service::control) {
                bool wrong_control=false;
                try{module.bind_control(std::make_shared<control_data::StartupData>(),[]{},application);}
                catch(const std::invalid_argument&){wrong_control=true;}assert(wrong_control);
            }
            if(role==Service::login) {
                auto startup=std::make_shared<LoginStartupData>();retained_startup=startup;
                bool duplicate=false;
                try{module.bind_login(startup,"missing-reference",application);}
                catch(const std::invalid_argument&){duplicate=true;}assert(duplicate);
                application.load_data={};
                hooks=module.bind_login(startup,"missing-reference",application);
            } else if(role==Service::control) {
                auto startup=std::make_shared<control_data::StartupData>();retained_control=startup;
                bool duplicate=false;
                try{module.bind_control(startup,[]{assert(false);},application);}
                catch(const std::invalid_argument&){duplicate=true;}assert(duplicate);
                application.load_data={};
                bool missing=false;
                try{module.bind_control(startup,{},application);}
                catch(const std::invalid_argument&){missing=true;}assert(missing);
                hooks=module.bind_control(startup,[]{assert(false);},application);
            } else if(role==Service::world) {
                auto startup=std::make_shared<world_data::WorldStartupData>();retained_world=startup;
                world_data::WorldStartupCallbacks callbacks;
                callbacks.schedule.batch=[](std::vector<BYTE>){assert(false);};
                callbacks.schedule.updated=[](WORD){assert(false);};
                callbacks.schedule.delete_database=[](WORD){assert(false);};
                callbacks.random=[](){assert(false);return 0;};
                callbacks.bounded_random=[](DWORD){assert(false);return DWORD(0);};
                callbacks.refund=[](DWORD,DWORD){assert(false);};callbacks.log=[](const CString&){assert(false);};
                bool duplicate=false;try{module.bind_world(startup,callbacks,application);}
                catch(const std::invalid_argument&){duplicate=true;}assert(duplicate);
                application.load_data={};
                bool missing=false;try{module.bind_world(startup,{},application);}
                catch(const std::invalid_argument&){missing=true;}assert(missing);
                hooks=module.bind_world(startup,callbacks,application);
            } else {
                bool wrong_role=false;
                try{module.bind_login(std::make_shared<LoginStartupData>(),"",application);}
                catch(const std::invalid_argument&){wrong_role=true;}assert(wrong_role);
                hooks=module.bind(application);
            }
            // No dummy defaults: missing application state stages are rejected.
            bool rejected=false;try{module.bind({});}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
        } // hooks must retain the real database object beyond the adapter lifetime
        if(role==Service::login) assert(!retained_startup.expired());
        if(role==Service::control) assert(!retained_control.expired());
        if(role==Service::world) assert(!retained_world.expired());
        {
            Runtime runtime(config,hooks);bool rejected=false;
            try{runtime.run();}catch(const RoleStartupError& error){assert(error.code()==EC_INITSERVICE_DBOPENFAILED);rejected=true;}
            assert(rejected && reads==1 && updated && runtime.port()==0 && runtime.finished());
        }
        hooks={};
        assert(retained_startup.expired() && retained_control.expired() && retained_world.expired());
    }
    std::cout<<"PASS owned four-role source database callbacks, credential-provider boundary and failure cleanup\n";
}
