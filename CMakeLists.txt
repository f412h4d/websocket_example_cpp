cmake_minimum_required(VERSION 3.10)
project(websocket-client-async-ssl)

set(CMAKE_CXX_STANDARD 11)

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(libs/common)

add_executable(websocket-client-async-ssl websocket_client_async_ssl.cpp)
target_link_libraries(websocket-client-async-ssl ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
