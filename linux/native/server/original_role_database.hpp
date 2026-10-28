#pragma once
#include "original_database_init.hpp"
#include "role_lifecycle.hpp"
#include "login_startup_data.hpp"
#include "control_startup_data.hpp"
#include "world_startup_data.hpp"
#include <cstdlib>
namespace fourstory::server {
// Owns the source database object for the full callback lifetime. Application
// state/loading remains explicit; this adapter installs only implemented source
// database/activation callbacks, never placeholder success stages.
class OriginalRoleDatabase {
public:
    using Password=std::function<std::string(const std::string&)>;
private:
    struct State {
        Config config;
        Password password;
        CSqlDatabase database;
        State(Config value,Password provider):config(std::move(value)),password(std::move(provider)) {}
        DWORD initialize() {
            const auto secret=password(config.db_password_env);
            const auto* dsn=config.db_dsn.c_str();const auto* user=config.db_user.c_str();
            switch(config.service) {
            case Service::control: return initialize_original_database<Service::control>(database,dsn,user,secret.c_str());
            case Service::login: return initialize_original_database<Service::login>(database,dsn,user,secret.c_str());
            case Service::world: return initialize_original_database<Service::world>(database,dsn,user,secret.c_str());
            case Service::map: return initialize_original_database<Service::map>(database,dsn,user,secret.c_str());
            }
            throw std::invalid_argument("Unknown database role");
        }
    };
    std::shared_ptr<State> state_;
public:
    explicit OriginalRoleDatabase(Config config,Password password=[](const std::string& name) {
        const auto* value=std::getenv(name.c_str());
        if(!value) throw std::runtime_error("Configured database password environment variable is missing");
        return std::string(value);
    }):state_(std::make_shared<State>(std::move(config),std::move(password))) {
        if(!state_->password) throw std::invalid_argument("Missing database password provider");
    }
    CSqlDatabase& database() const noexcept {return state_->database;}
    Runtime::Hooks bind_control(std::shared_ptr<control_data::StartupData> startup,
                               std::function<void()> platform,RoleLifecycle::Callbacks application,
                               Runtime::Hooks transport={}) const {
        if(state_->config.service!=Service::control || !startup || !platform || application.load_data)
            throw std::invalid_argument("Invalid original Control startup binding");
        const auto state=state_;
        application.load_data=[state,startup,platform=std::move(platform)] {
            return LoadOriginalControlStartup(&state->database,startup.get(),platform);
        };
        return bind(std::move(application),std::move(transport));
    }
    Runtime::Hooks bind_world(std::shared_ptr<world_data::WorldStartupData> startup,
                             world_data::WorldStartupCallbacks callbacks,
                             RoleLifecycle::Callbacks application,Runtime::Hooks transport={}) const {
        if(state_->config.service!=Service::world || !startup || application.load_data ||
           !callbacks.schedule.batch || !callbacks.schedule.updated || !callbacks.schedule.delete_database ||
           !callbacks.random || !callbacks.bounded_random || !callbacks.refund || !callbacks.log)
            throw std::invalid_argument("Invalid original World startup binding");
        const auto state=state_;
        application.load_data=[state,startup,callbacks=std::move(callbacks)] {
            return LoadOriginalWorldStartup(&state->database,startup.get(),callbacks);
        };
        return bind(std::move(application),std::move(transport));
    }
    Runtime::Hooks bind_login(std::shared_ptr<LoginStartupData> startup,std::string reference_path,
                             RoleLifecycle::Callbacks application,Runtime::Hooks transport={}) const {
        if(state_->config.service!=Service::login || !startup || application.load_data)
            throw std::invalid_argument("Invalid original Login startup binding");
        const auto state=state_;
        application.load_data=[state,startup,reference_path=std::move(reference_path)] {
            return LoadOriginalLoginStartup(&state->database,startup.get(),reference_path.c_str());
        };
        return bind(std::move(application),std::move(transport));
    }
    Runtime::Hooks bind(RoleLifecycle::Callbacks application,Runtime::Hooks transport={}) const {
        if(application.init_database || application.close_database ||
           (state_->config.service==Service::login && application.clear_login_users))
            throw std::invalid_argument("Duplicate original database callbacks");
        const auto state=state_;
        application.init_database=[state]{return state->initialize();};
        application.close_database=[state]{state->database.Close();};
        if(state->config.service==Service::login)
            application.clear_login_users=[state]{ClearOriginalLoginUsers(&state->database);};
        return RoleLifecycle(state->config.service,std::move(application)).bind(std::move(transport));
    }
};
}
