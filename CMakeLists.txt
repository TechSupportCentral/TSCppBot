cmake_minimum_required(VERSION 3.30)
project(TSCppBot)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

file(GLOB COMMAND_MODULE_SOURCE "command_modules/*.cpp")
add_executable(TSCppBot main.cpp util.cpp ${COMMAND_MODULE_SOURCE})

find_package(DPP REQUIRED)
target_link_libraries(TSCppBot ${DPP_LIBRARIES})
target_include_directories(TSCppBot PRIVATE ${DPP_INCLUDE_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE REQUIRED sqlite3)
target_link_libraries(TSCppBot ${SQLITE_LIBRARIES})
target_include_directories(TSCppBot PRIVATE ${SQLITE_INCLUDE_DIR})

set_target_properties(TSCppBot PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
)