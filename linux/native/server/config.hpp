#pragma once
#include <arpa/inet.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <istream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace fourstory::server {
enum class Service { control, login, world, map };
enum class DependencyLoss { stop, retry };
struct Endpoint { std::string address; std::uint16_t port = 0; };
struct Config {
    Service service = Service::control;
    Endpoint listen{"127.0.0.1", 0}, world, log;
    std::uint8_t server_id = 0, group_id = 0;
    bool auto_start = false;
    DependencyLoss dependency_loss = DependencyLoss::stop;
    std::string db_dsn, db_user, db_password_env;
    unsigned workers = 2, queue_limit = 1024;
    unsigned reconnect_initial_ms = 250, reconnect_max_ms = 10000, shutdown_ms = 5000;
};
inline std::string trim(std::string s) {
    auto space=[](unsigned char c){ return std::isspace(c)!=0; };
    s.erase(s.begin(),std::find_if_not(s.begin(),s.end(),space));
    s.erase(std::find_if_not(s.rbegin(),s.rend(),space).base(),s.end());
    return s;
}
inline Config read_config(std::istream& input) {
    const std::set<std::string> allowed={"service","listen_address","port","server_id","group_id",
        "world_address","world_port","log_address","log_port","db_dsn","db_user","db_password_env",
        "workers","queue_limit","reconnect_initial_ms","reconnect_max_ms","shutdown_ms","auto_start","dependency_loss"};
    std::map<std::string,std::string> values;
    std::string line; unsigned line_number=0;
    while(std::getline(input,line)) {
        ++line_number;line=trim(line);
        if(line.empty() || line[0]=='#') continue;
        const auto equals=line.find('=');
        if(equals==std::string::npos) throw std::runtime_error("Invalid config line "+std::to_string(line_number));
        const auto key=trim(line.substr(0,equals)), value=trim(line.substr(equals+1));
        if(!allowed.count(key) || value.empty()) throw std::runtime_error("Unknown key or empty value at line "+std::to_string(line_number));
        if(!values.emplace(key,value).second) throw std::runtime_error("Duplicate config key at line "+std::to_string(line_number));
    }
    if(input.bad()) throw std::runtime_error("Cannot read configuration");
    auto required=[&](const std::string& key)->std::string {
        const auto found=values.find(key);
        if(found==values.end()) throw std::runtime_error("Missing config key: "+key);
        return found->second;
    };
    auto number=[&](const std::string& key,unsigned minimum,unsigned maximum,unsigned fallback,bool mandatory=false) {
        const auto found=values.find(key);
        if(found==values.end()) { if(mandatory) required(key); return fallback; }
        unsigned result=0;
        for(unsigned char c:found->second) {
            if(c<'0' || c>'9' || result>(maximum-(c-'0'))/10)
                throw std::runtime_error("Invalid numeric config key: "+key);
            result=result*10+(c-'0');
        }
        if(result<minimum || result>maximum) throw std::runtime_error("Out-of-range config key: "+key);
        return result;
    };
    auto address=[&](const std::string& key,std::string fallback) {
        const auto found=values.find(key);if(found!=values.end()) fallback=found->second;
        in_addr parsed{};
        if(inet_pton(AF_INET,fallback.c_str(),&parsed)!=1) throw std::runtime_error("Invalid IPv4 config key: "+key);
        return fallback;
    };
    Config config;
    const auto service=required("service");
    if(service=="control") config.service=Service::control;
    else if(service=="login") config.service=Service::login;
    else if(service=="world") config.service=Service::world;
    else if(service=="map") config.service=Service::map;
    else throw std::runtime_error("Invalid service config key");
    config.listen={address("listen_address","127.0.0.1"),static_cast<std::uint16_t>(number("port",1,65535,0,true))};
    config.server_id=static_cast<std::uint8_t>(number("server_id",0,255,0,config.service!=Service::control));
    config.group_id=static_cast<std::uint8_t>(number("group_id",0,255,0,
        config.service==Service::map || config.service==Service::world));
    config.auto_start=number("auto_start",0,1,0)!=0;
    if(values.count("dependency_loss")) {
        if(values.at("dependency_loss")=="retry") config.dependency_loss=DependencyLoss::retry;
        else if(values.at("dependency_loss")!="stop") throw std::runtime_error("Invalid dependency loss policy");
    }
    config.db_dsn=required("db_dsn");config.db_user=required("db_user");config.db_password_env=required("db_password_env");
    for(std::size_t i=0;i<config.db_password_env.size();++i) {
        const unsigned char c=config.db_password_env[i];
        if(!(std::isalpha(c) || c=='_' || (i>0 && std::isdigit(c))))
            throw std::runtime_error("Invalid database password environment name");
    }
    if(config.service==Service::map || values.count("world_address") || values.count("world_port")) {
        required("world_address");
        config.world={address("world_address",""),static_cast<std::uint16_t>(number("world_port",1,65535,0,true))};
    }
    // Map's LoadConfig reads LogIP/LogPort unconditionally in this release.
    // Login gates them behind DEF_UDPLOG (disabled in the baseline source).
    if(config.service==Service::map || values.count("log_address") || values.count("log_port")) {
        required("log_address");
        config.log={address("log_address",""),static_cast<std::uint16_t>(number("log_port",1,65535,0,true))};
    }
    config.workers=number("workers",1,128,2);
    config.queue_limit=number("queue_limit",1,1000000,1024);
    config.reconnect_initial_ms=number("reconnect_initial_ms",1,600000,250);
    config.reconnect_max_ms=number("reconnect_max_ms",1,600000,10000);
    config.shutdown_ms=number("shutdown_ms",1,600000,5000);
    if(config.reconnect_max_ms<config.reconnect_initial_ms) throw std::runtime_error("Invalid reconnect interval ordering");
    return config;
}
}
