# NovaChat 自研 RPC 框架（原 mprpc）+ ZooKeeper 发现
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src RPC_SRC)
add_library(rpc ${RPC_SRC})
target_include_directories(rpc PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/include
  ${NOVA_DEPS_PREFIX}/include
)
target_link_directories(rpc PUBLIC
  ${NOVA_DEPS_PREFIX}/lib
  ${NOVA_DEPS_PREFIX}/lib/x86_64-linux-gnu
)
target_link_libraries(rpc muduo_net muduo_base protobuf zookeeper_mt pthread)
