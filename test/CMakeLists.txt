cmake_minimum_required(VERSION 3.14)
project(tests)

add_subdirectory(3rdparty/googletest)
add_subdirectory(3rdparty/opponents/Steamhammer)
add_subdirectory(3rdparty/opponents/Locutus)

include_directories(${CMAKE_SOURCE_DIR}/3rdparty/googletest/include)

include_directories(SYSTEM
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/Util/Source
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPI/openbw
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPI/openbw/BWAPI
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPI/Source
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/BWAPICore
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/OpenBWData
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/bwapi/bwapi/Shared
        ${CMAKE_SOURCE_DIR}/3rdparty/openbw/openbw
)

set(TEST_INCLUDES
        ${PROJECT_SOURCE_DIR}
        )
include_directories(${TEST_INCLUDES})

# Common harness shared by all opponent runners (SRP: game loop + maps only,
# no dependency on any concrete bot). New bots reuse this without touching
# existing runners (OCP).
add_library(test_common STATIC
        BWTest.cpp
        Maps.cpp
)
target_link_libraries(test_common PUBLIC gtest BWAPI BWAPILIB DemoAIModule)
target_compile_options(test_common PRIVATE -Wall -Werror)
if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(test_common PUBLIC DEBUG=1)
endif()

# One runner per opponent family, so conflicting bot globals (e.g. Config::*,
# BOSS statics duplicated in Steamhammer and Locutus) never coexist in one
# process. Adding bot N = new executable below, no changes to existing ones.
# Future non-linkable bots (.dll/.exe/.jar) get their own runner that spawns
# them instead of linking (DIP: BWTest depends on an AIModule factory).
function(add_opponent_tests target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE test_common gtest gtest_main BWAPI BWAPILIB DemoAIModule)
    target_compile_options(${target} PRIVATE -Wall -Werror)
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        target_compile_definitions(${target} PUBLIC DEBUG=1)
    endif()
endfunction()

add_opponent_tests(tests-steamhammer Steamhammer.cpp RushDefense.cpp)
target_link_libraries(tests-steamhammer PRIVATE Steamhammer)

add_opponent_tests(tests-locutus Locutus.cpp)
target_link_libraries(tests-locutus PRIVATE Locutus)

file(COPY ${CMAKE_SOURCE_DIR}/test/3rdparty/maps DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bwapi-data/AI)
file(COPY ${CMAKE_SOURCE_DIR}/test/3rdparty/bwta DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bwapi-data/AI)
file(COPY ${CMAKE_SOURCE_DIR}/test/3rdparty/opponents/Steamhammer/Steamhammer_2.4.2.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bwapi-data/AI)
file(COPY ${CMAKE_SOURCE_DIR}/test/3rdparty/opponents/Locutus/Locutus.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bwapi-data/AI)
