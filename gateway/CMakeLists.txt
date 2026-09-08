# Gateway：长连接接入 + Presence RPC 客户端
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src GATEWAY_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/db GATEWAY_DB)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/model GATEWAY_MODEL)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/redis GATEWAY_REDIS)

add_executable(NovaGateway
  ${GATEWAY_SRC}
  ${GATEWAY_DB}
  ${GATEWAY_MODEL}
  ${GATEWAY_REDIS}
  ${CMAKE_SOURCE_DIR}/proto/common.pb.cc
  ${CMAKE_SOURCE_DIR}/proto/presence.pb.cc
)

target_include_directories(NovaGateway PRIVATE
  ${CMAKE_SOURCE_DIR}/gateway/include
  ${CMAKE_SOURCE_DIR}/gateway/include/db
  ${CMAKE_SOURCE_DIR}/gateway/include/model
  ${CMAKE_SOURCE_DIR}/gateway/include/redis
  ${CMAKE_SOURCE_DIR}/thirdparty
  ${CMAKE_SOURCE_DIR}/proto
  ${CMAKE_SOURCE_DIR}/rpc/src/include
  ${NOVA_DEPS_PREFIX}/include
)

target_link_libraries(NovaGateway
  rpc
  muduo_net muduo_base muduo_http
  mysqlclient
  hiredis
  protobuf
  pthread
  ssl crypto dl z resolv
)
