#pragma once
#include "../original_database_init.hpp"
#include "../role_lifecycle.hpp"
#include <cassert>
namespace fourstory::server::tests {
template<Service Role> void check_original_database_lifecycle() {
    CSqlDatabase database;bool closed=false;
    RoleLifecycle::Callbacks callbacks;
    callbacks.init_database=[&]{return initialize_original_database<Role>(database,"4Story_Nonexistent_Test_DSN","","");};
    // This is a failure-path fixture: none of the later game stages may execute.
    callbacks.load_data=[] {assert(false);return 0u;};
    callbacks.install_workers=[](Runtime&){assert(false);};
    callbacks.init_environment=[](Runtime&){assert(false);};
    callbacks.clear_login_users=[] {assert(false);};
    callbacks.tournament_update_if_needed=[] {assert(false);};
    callbacks.save_characters=[](Runtime&){assert(false);};
    callbacks.save_pvp=[](Runtime&){assert(false);};
    callbacks.update_data=[&]{database.ClearQuery();};
    callbacks.close_database=[&]{database.Close();closed=true;};
    Config config;config.service=Role;config.listen={"127.0.0.1",0};
    Runtime runtime(config,RoleLifecycle(Role,callbacks).bind());
    bool rejected=false;
    try{runtime.run();}catch(const RoleStartupError& error){assert(error.code()==EC_INITSERVICE_DBOPENFAILED);rejected=true;}
    assert(rejected && closed && runtime.finished() && runtime.port()==0 && !database.IsOpen());
}
}
