cmake_minimum_required(VERSION 3.10)
project(NovaChat CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED NOVA_DEPS_PREFIX)
  set(NOVA_DEPS_PREFIX "$ENV{HOME}/deps")
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

include_directories(
  ${NOVA_DEPS_PREFIX}/include
  ${PROJECT_SOURCE_DIR}/rpc/src/include
  ${PROJECT_SOURCE_DIR}/proto
  ${PROJECT_SOURCE_DIR}/gateway/include
  ${PROJECT_SOURCE_DIR}/gateway/include/db
  ${PROJECT_SOURCE_DIR}/gateway/include/model
  ${PROJECT_SOURCE_DIR}/gateway/include/redis
  ${PROJECT_SOURCE_DIR}/thirdparty
)

link_directories(
  ${NOVA_DEPS_PREFIX}/lib
  ${NOVA_DEPS_PREFIX}/lib/x86_64-linux-gnu
  ${PROJECT_SOURCE_DIR}/lib
)

# 自研 RPC + UserService（ZK 服务级注册；Gateway 调 GetUserProfile）
option(NOVA_BUILD_RPC "Build rpc framework and UserService" ON)

if(NOVA_BUILD_RPC)
  add_subdirectory(rpc)
  add_subdirectory(services/user)
endif()

add_subdirectory(gateway)
add_subdirectory(client)
