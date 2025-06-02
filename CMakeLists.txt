cmake_minimum_required(VERSION 3.30)
if(WIN32)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake)
endif()

project(TSCppBot)
file(GLOB COMMAND_MODULE_SOURCE "src/command_modules/*.cpp")
add_executable(TSCppBot src/main.cpp src/util.cpp ${COMMAND_MODULE_SOURCE})

if(WIN32)
    find_package(dpp CONFIG REQUIRED)
    find_package(unofficial-sqlite3 CONFIG REQUIRED)
    target_link_libraries(TSCppBot PRIVATE dpp::dpp unofficial::sqlite3::sqlite3)
else()
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

    find_package(DPP REQUIRED)
    target_link_libraries(TSCppBot ${DPP_LIBRARIES})
    target_include_directories(TSCppBot PRIVATE ${DPP_INCLUDE_DIR})

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SQLITE REQUIRED sqlite3)
    target_link_libraries(TSCppBot ${SQLITE_LIBRARIES})
    target_include_directories(TSCppBot PRIVATE ${SQLITE_INCLUDE_DIR})
endif()

set_target_properties(TSCppBot PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)