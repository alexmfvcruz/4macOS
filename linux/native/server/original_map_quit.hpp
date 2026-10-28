#pragma once
#include "runtime.hpp"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/ErrorCode.h"
#ifndef FOURSTORY_NATIVE_SERVER
#error "Original Map quit boundary requires FOURSTORY_NATIVE_SERVER"
#endif
namespace fourstory::server {
inline std::uint32_t original_map_quit(Runtime& runtime,const std::function<void(const char*)>& log) {
    if(!log) throw std::invalid_argument("Missing Map service log callback");
    const auto& LogSmartly=log;
#ifndef _T
#define FOURSTORY_QUIT_LOCAL_T
#define _T(value) value
#endif
#include "../../source/4Story 5.0 Source Files/Server/TMapSvr/QuitService.inc"
#ifdef FOURSTORY_QUIT_LOCAL_T
#undef _T
#undef FOURSTORY_QUIT_LOCAL_T
#endif
}
}
