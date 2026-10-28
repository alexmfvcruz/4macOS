#include "original_database_lifecycle.hpp"
#include "../login_startup_data.hpp"
#include "../control_manager_login.hpp"
#include <iostream>
using namespace fourstory::server;
int main() {
    tests::check_original_database_lifecycle<Service::control>();
    tests::check_original_database_lifecycle<Service::login>();
    tests::check_original_database_lifecycle<Service::world>();
    tests::check_original_database_lifecycle<Service::map>();
    CSqlDatabase database;
    BYTE authority=255;
    assert(!AuthenticateOriginalControlManager(&database,"name","password",&authority) && authority==0);
    ControlManagerAuthority manager([]{});assert(manager.authenticated_login(MANAGER_ALL));
    assert(!authenticate_control_manager(database,manager,"name","password") && !manager.authenticated());
    assert(!InitQueryTControlSvr(&database));assert(!InitQueryTLoginSvr(&database));
    assert(!InitQueryTWorldSvr(&database));assert(!InitQueryTMapSvr(&database));
    assert(database.GetQuery("CTBLGroup")==nullptr);
    LoginStartupData startup;
    assert(LoadOriginalLoginStartup(&database,&startup,nullptr)==EC_INITSERVICE_PREPAREQUERY);
    assert(startup.nation==0 && startup.current_users.empty() && startup.veterans.empty());
    ClearOriginalLoginUsers(&database);
    bool rejected=false;try{ClearOriginalLoginUsers(nullptr);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    std::cout<<"PASS all four original query implementations linked together and real InitDB lifecycle failures\n";
}
