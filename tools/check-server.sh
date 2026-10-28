#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"
mkdir -p build/server
odbc_prefix=$(brew --prefix unixodbc)
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  native/server/tests/odbc_platform_test.cpp -L"$odbc_prefix/lib" -lodbc \
  -o build/server/odbc_platform_test
build/server/odbc_platform_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/original_odbc_test.cpp \
  'source/4Story 5.0 Source Files/Server/TNetLib/SqlDatabase.cpp' \
  'source/4Story 5.0 Source Files/Server/TNetLib/SqlBase.cpp' \
  'source/4Story 5.0 Source Files/Server/TNetLib/SqlQuery.cpp' \
  'source/4Story 5.0 Source Files/Server/TNetLib/SqlDirect.cpp' \
  'source/4Story 5.0 Source Files/Server/TNetLib/BindDesc.cpp' \
  -L"$odbc_prefix/lib" -lodbc -o build/server/original_odbc_test
build/server/original_odbc_test
for test_name in odbc_binding login_queries login_cleanup TControlSvr_queries TWorldSvr_queries TMapSvr_queries; do
  set --
  case "$test_name" in
    odbc_binding) ;;
    login_queries|login_cleanup) set -- 'source/4Story 5.0 Source Files/Server/TLoginSvr/DBAccess.cpp' ;;
    *) set -- "source/4Story 5.0 Source Files/Server/${test_name%_queries}/DBAccess.cpp" ;;
  esac
  clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
    -Wno-missing-declarations -pthread -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "native/server/tests/${test_name}_test.cpp" "$@" \
    native/server/world_guild.cpp \
    'source/4Story 5.0 Source Files/Server/TNetLib/Packet.cpp' \
    'source/4Story 5.0 Source Files/Server/TNetLib/SqlDatabase.cpp' \
    'source/4Story 5.0 Source Files/Server/TNetLib/SqlBase.cpp' \
    'source/4Story 5.0 Source Files/Server/TNetLib/SqlQuery.cpp' \
    -L"$odbc_prefix/lib" -lodbc -o "build/server/${test_name}_test"
  "build/server/${test_name}_test"
done
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/periodic_test.cpp -o build/server/periodic_test
build/server/periodic_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/role_workers_test.cpp -o build/server/role_workers_test
build/server/role_workers_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread -DFOURSTORY_NATIVE_PACKET \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/original_shared_test.cpp \
  'source/4Story 5.0 Source Files/Server/TNetLib/Packet.cpp' \
  -o build/server/original_shared_test
build/server/original_shared_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/debug_channel_test.cpp -o build/server/debug_channel_test
build/server/debug_channel_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -DFOURSTORY_NATIVE_PACKET \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/original_packet_test.cpp \
  'source/4Story 5.0 Source Files/Server/TNetLib/Packet.cpp' \
  -o build/server/original_packet_test
build/server/original_packet_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/legacy_string_test.cpp -o build/server/legacy_string_test
build/server/legacy_string_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/database_time_test.cpp -o build/server/database_time_test
build/server/database_time_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/config_test.cpp -o build/server/config_test
build/server/config_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/work_queue_test.cpp -o build/server/work_queue_test
build/server/work_queue_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/worker_loop_test.cpp -o build/server/worker_loop_test
build/server/worker_loop_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/transport_test.cpp -o build/server/transport_test
build/server/transport_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/session_test.cpp -o build/server/session_test
build/server/session_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -DFOURSTORY_NATIVE_PACKET native/server/tests/network_test.cpp \
  'source/4Story 5.0 Source Files/Server/TNetLib/Packet.cpp' \
  -o build/server/network_test
build/server/network_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/runtime_test.cpp -o build/server/runtime_test
build/server/runtime_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/role_lifecycle_test.cpp -o build/server/role_lifecycle_test
build/server/role_lifecycle_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 -pthread \
  -Wno-missing-declarations -DFOURSTORY_NATIVE_PACKET -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/role_timers_test.cpp \
  'source/4Story 5.0 Source Files/Server/TNetLib/Packet.cpp' \
  -o build/server/role_timers_test
build/server/role_timers_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread -DFOURSTORY_NATIVE_SERVER \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/map_quit_test.cpp -o build/server/map_quit_test
build/server/map_quit_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/client_reference_file_test.cpp -o build/server/client_reference_file_test
build/server/client_reference_file_test
for role in control world login map; do
  clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 -pthread \
    -Wno-missing-declarations -DSKYGARDEN -DFOURSTORY_NATIVE_PACKET -fsanitize=address,undefined -fno-omit-frame-pointer \
    "native/server/tests/${role}_dispatch_test.cpp" \
    'source/4Story 5.0 Source Files/Server/TNetLib/Packet.cpp' \
    -o "build/server/${role}_dispatch_test"
  "build/server/${role}_dispatch_test"
done
sh tools/build-server.sh
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 -pthread \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  native/server/tests/shared_library_test.cpp build/server/libfourstory-shared.a \
  -L"$odbc_prefix/lib" -lodbc -o build/server/shared_library_test
TZ=UTC build/server/shared_library_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 -pthread \
  -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  native/server/tests/combined_roles_test.cpp build/server/libfourstory-roles.a \
  build/server/libfourstory-shared.a -L"$odbc_prefix/lib" -lodbc \
  -o build/server/combined_roles_test
build/server/combined_roles_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 -Wno-missing-declarations -pthread \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  native/server/tests/control_startup_test.cpp build/server/libfourstory-roles.a \
  build/server/libfourstory-shared.a -L"$odbc_prefix/lib" -lodbc \
  -o build/server/control_startup_test
build/server/control_startup_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/world_data_types_test.cpp 'source/4Story 5.0 Source Files/Server/TNetLib/Packet.cpp' \
  -o build/server/world_data_types_test
build/server/world_data_types_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/world_guild_test.cpp native/server/world_guild.cpp \
  -o build/server/world_guild_test
TZ=UTC build/server/world_guild_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/world_database_item_test.cpp -o build/server/world_database_item_test
build/server/world_database_item_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/world_battle_schedule_test.cpp -o build/server/world_battle_schedule_test
build/server/world_battle_schedule_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/world_startup_helpers_test.cpp native/server/world_guild.cpp \
  -o build/server/world_startup_helpers_test
build/server/world_startup_helpers_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/world_guild_recruitment_test.cpp native/server/world_guild.cpp \
  'source/4Story 5.0 Source Files/Server/TNetLib/Packet.cpp' \
  -o build/server/world_guild_recruitment_test
build/server/world_guild_recruitment_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/map_skill_template_test.cpp native/server/map_skill_template.cpp \
  -o build/server/map_skill_template_test
build/server/map_skill_template_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  native/server/tests/map_skill_template_test.cpp build/server/libfourstory-roles.a \
  -o build/server/map_skill_archive_test
build/server/map_skill_archive_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/map_quest_state_test.cpp -o build/server/map_quest_state_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/map_quest_loading_test.cpp build/server/libfourstory-roles.a \
  build/server/libfourstory-shared.a -L"$odbc_prefix/lib" -lodbc \
  -o build/server/map_quest_loading_test
build/server/map_quest_loading_test
build/server/map_quest_state_test
for component in world_battle_modules world_rank_state world_startup_data; do
  clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
    -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "native/server/tests/${component}_test.cpp" native/server/world_guild.cpp \
    'source/4Story 5.0 Source Files/Server/TNetLib/Packet.cpp' \
    -o "build/server/${component}_test"
  "build/server/${component}_test"
done
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/world_event_time_test.cpp -o build/server/world_event_time_test
TZ=UTC build/server/world_event_time_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/world_tournament_state_test.cpp -o build/server/world_tournament_state_test
build/server/world_tournament_state_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/server/tests/world_tournament_calendar_test.cpp 'source/4Story 5.0 Source Files/Server/TNetLib/Packet.cpp' \
  -o build/server/world_tournament_calendar_test
TZ=UTC build/server/world_tournament_calendar_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 -Wno-missing-declarations -pthread \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  native/server/tests/world_database_actions_test.cpp build/server/libfourstory-roles.a \
  build/server/libfourstory-shared.a -L"$odbc_prefix/lib" -lodbc \
  -o build/server/world_database_actions_test
build/server/world_database_actions_test
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 -pthread \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  native/server/tests/original_role_database_test.cpp build/server/libfourstory-roles.a \
  build/server/libfourstory-shared.a -L"$odbc_prefix/lib" -lodbc \
  -o build/server/original_role_database_test
build/server/original_role_database_test
python3 native/server/tests/query_symbols_test.py
python3 native/server/tests/process_test.py
python3 native/server/tests/role_process_test.py
python3 native/server/tests/supervisor_test.py
python3 native/server/tests/supervisor_control_test.py
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-missing-declarations -Wno-invalid-utf8 \
  -pthread -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_PACKET \
  native/server/tests/control_management_test.cpp build/server/libfourstory-shared.a \
  -o build/server/control_management_test
python3 native/server/tests/control_management_test.py
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-missing-declarations \
  -Wno-invalid-utf8 -pthread -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_PACKET \
  native/server/tests/control_status_transition_test.cpp build/server/libfourstory-shared.a \
  -o build/server/control_status_transition_test
build/server/control_status_transition_test
# Complete original modules compiled natively (milestone 4).
sh tools/build-modules.sh
module_build=build/server/module
module_sources='source/4Story 5.0 Source Files/Server/TNetLib'
clang++ -std=c++17 -g -DFOURSTORY_NATIVE_MODULE -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_ODBC \
  -DFOURSTORY_SERVICE_NAME='"compat-test"' -w -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer \
  -Inative/server/win32/include -I"$odbc_prefix/include" -I"$module_sources" \
  -I'source/4Story 5.0 Source Files/Lib/Own/TProtocol/include' \
  native/server/tests/win32_compat_test.cpp native/server/win32/win32.cpp native/server/win32/crypto.cpp \
  "$module_sources/Packet.cpp" "$module_sources/Session.cpp" "$module_sources/SqlBase.cpp" \
  "$module_sources/SqlDatabase.cpp" "$module_sources/SqlQuery.cpp" "$module_sources/SqlDirect.cpp" \
  "$module_sources/BindDesc.cpp" -L"$odbc_prefix/lib" -lodbc -o "$module_build/win32_compat_test"
"$module_build/win32_compat_test"
# Windows view of every original file changed since the module work began.
bash tools/check-windows-branches.sh f0dc152
# Live four-role cluster against the isolated audit SQL Server copies.
cluster_python=deps/python/database/bin/python
if [ -x "$cluster_python" ] && [ -f runtime/server-dev/sql-server.env ] && nc -z 127.0.0.1 14330 2>/dev/null; then
  "$cluster_python" native/server/tests/native_cluster_test.py
else
  echo "SKIP live native cluster: isolated SQL Server or cluster venv unavailable (see research/database/native-cluster.md)"
fi
