#pragma once
#include "world_data_types.hpp"
#include <functional>
namespace fourstory::server {
struct WorldDatabaseActionResult {DWORD handler_code;bool executed;};
using WorldDatabaseBatch=std::function<void(std::vector<BYTE>)>;
WorldDatabaseActionResult ExecuteOriginalWorldTournamentPayback(CSqlDatabase*,PACKETBUF*,const WorldDatabaseBatch&);
WorldDatabaseActionResult ExecuteOriginalWorldTournamentClear(CSqlDatabase*);
WorldDatabaseActionResult ExecuteOriginalWorldTournamentDelete(CSqlDatabase*,PACKETBUF*);
}
