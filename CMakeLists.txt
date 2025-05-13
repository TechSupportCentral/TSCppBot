cmake_minimum_required(VERSION 3.30)
project(TSCppBot)

set(CMAKE_CXX_STANDARD 17)

include_directories(/opt/homebrew/include)
link_libraries(/opt/homebrew/lib/libdpp.dylib)
link_libraries(/opt/homebrew/Cellar/sqlite/3.49.1/lib/libsqlite3.3.49.1.dylib)
add_executable(TSCppBot main.cpp
                        util.cpp
                        command_modules/meta.cpp
                        command_modules/server_info.cpp
                        command_modules/db_commands.cpp)
