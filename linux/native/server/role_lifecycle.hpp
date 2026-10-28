#pragma once
#include "runtime.hpp"
#include <cstdint>
#include <string>

namespace fourstory::server {
class RoleStartupError : public std::runtime_error {
    std::uint32_t code_;
public:
    RoleStartupError(const char* stage,std::uint32_t code)
        :std::runtime_error(std::string(stage)+" failed, code="+std::to_string(code)),code_(code) {}
    std::uint32_t code() const noexcept {return code_;}
};
// Module callbacks run on the owner unless they explicitly queue worker work.
// Partial initialization cleanup must be safe; close_database always follows
// update_data, even when that callback throws. No missing callback succeeds.
class RoleLifecycle {
public:
    struct Callbacks {
        std::function<std::uint32_t()> init_database,load_data;
        std::function<void(Runtime&)> install_workers,init_environment;
        std::function<void()> clear_login_users,tournament_update_if_needed;
        std::function<void(Runtime&)> save_characters,save_pvp;
        std::function<void()> update_data,close_database;
    };
private:
    struct State {bool database_attempted=false,loaded=false,active=false,closed=false;};
    Service role_;
    Callbacks callbacks_;
public:
    RoleLifecycle(Service role,Callbacks callbacks):role_(role),callbacks_(std::move(callbacks)) {
        if(!callbacks_.init_database || !callbacks_.load_data || !callbacks_.install_workers ||
           !callbacks_.update_data || !callbacks_.close_database ||
           (role==Service::login && !callbacks_.clear_login_users) ||
           (role==Service::world && (!callbacks_.tournament_update_if_needed || !callbacks_.save_pvp)) ||
           (role==Service::map && (!callbacks_.init_environment || !callbacks_.save_characters)))
            throw std::invalid_argument("Incomplete original role lifecycle callbacks");
    }
    Runtime::Hooks bind(Runtime::Hooks transport={}) const {
        auto state=std::make_shared<State>();auto callbacks=callbacks_;const auto role=role_;
        auto before=transport.prepare;
        transport.prepare=[state,callbacks,before,role](Runtime& runtime) {
            if(runtime.service()!=role)
                throw std::invalid_argument("Original lifecycle role does not match runtime service");
            state->database_attempted=true;
            auto code=callbacks.init_database();if(code) throw RoleStartupError("InitDB",code);
            code=callbacks.load_data();if(code) throw RoleStartupError("LoadData",code);
            state->loaded=true;callbacks.install_workers(runtime);
            if(before) before(runtime);

        };
        auto ready=transport.network_ready;
        transport.network_ready=[callbacks,role,ready](Runtime& runtime) {
            if(ready) ready(runtime);
            if(role==Service::map && !runtime.cancelled()) callbacks.init_environment(runtime);
        };
        auto activated=transport.activated;
        transport.activated=[state,callbacks,role,activated](Runtime& runtime) {
            state->active=true;
            if(role==Service::login) callbacks.clear_login_users();
            if(role==Service::world) callbacks.tournament_update_if_needed();
            if(activated) activated(runtime);
        };
        auto quiescing=transport.quiescing;
        transport.quiescing=[state,callbacks,role,quiescing](Runtime& runtime) {
            std::exception_ptr failure;
            try {
                if(state->loaded && state->active) {
                    if(role==Service::map) callbacks.save_characters(runtime);
                    if(role==Service::world) callbacks.save_pvp(runtime);
                }
            } catch(...) {failure=std::current_exception();}
            try {if(quiescing) quiescing(runtime);}catch(...) {if(!failure) failure=std::current_exception();}
            if(failure) std::rethrow_exception(failure);
        };
        auto stopped=transport.stopped;
        transport.stopped=[state,callbacks,stopped](Runtime& runtime) {
            std::exception_ptr failure;
            if(state->database_attempted && !state->closed) {
                state->closed=true;
                try {callbacks.update_data();}catch(...) {failure=std::current_exception();}
                try {callbacks.close_database();}catch(...) {if(!failure) failure=std::current_exception();}
            }
            try {if(stopped) stopped(runtime);}catch(...) {if(!failure) failure=std::current_exception();}
            if(failure) std::rethrow_exception(failure);
        };
        return transport;
    }
};
}
