cmake_minimum_required(VERSION 3.16)
project(EmbeddedGatewayFreertosProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(protocol_common
    common/protocol.cpp
)
target_include_directories(protocol_common PUBLIC common)

add_executable(device_sim
    device_sim/main.cpp
)
target_link_libraries(device_sim PRIVATE protocol_common)

add_executable(gateway
    gateway/main.cpp
)
target_link_libraries(gateway PRIVATE protocol_common)

add_executable(protocol_tests
    tests/protocol_tests.cpp
)
target_link_libraries(protocol_tests PRIVATE protocol_common)

add_executable(in_process_integration
    tests/in_process_integration.cpp
)
target_link_libraries(in_process_integration PRIVATE protocol_common)

if (WIN32)
    target_link_libraries(device_sim PRIVATE ws2_32)
    target_link_libraries(gateway PRIVATE ws2_32)
endif()
