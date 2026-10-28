#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"
mkdir -p build/server
odbc_prefix=$(brew --prefix unixodbc)
shared_dir='source/4Story 5.0 Source Files/Server/TNetLib'
objects=''
for unit in Packet SqlDatabase SqlBase SqlQuery SqlDirect BindDesc; do
  clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 -pthread \
    -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
    -c "$shared_dir/$unit.cpp" -o "build/server/$unit.o"
  objects="$objects build/server/$unit.o"
done
role_objects=''
for role in TControlSvr TLoginSvr TWorldSvr TMapSvr; do
  clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
    -Wno-missing-declarations -DSKYGARDEN -pthread -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC \
    -I"$odbc_prefix/include" \
    -c "source/4Story 5.0 Source Files/Server/$role/DBAccess.cpp" \
    -o "build/server/$role-queries.o"
  role_objects="$role_objects build/server/$role-queries.o"
done
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 -pthread \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -c native/server/world_guild.cpp -o build/server/world_guild.o
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 -pthread \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  -c native/server/map_skill_template.cpp -o build/server/map_skill_template.o
ar rcs build/server/libfourstory-roles.a $role_objects build/server/world_guild.o build/server/map_skill_template.o
# Object paths are controlled here and contain no whitespace.
ar rcs build/server/libfourstory-shared.a $objects
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Wno-invalid-utf8 \
  -Wno-missing-declarations -DSKYGARDEN -pthread \
  -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_SERVER -DFOURSTORY_NATIVE_ODBC -I"$odbc_prefix/include" \
  native/server/main.cpp build/server/libfourstory-shared.a \
  -L"$odbc_prefix/lib" -lodbc -o build/server/fourstory-service
