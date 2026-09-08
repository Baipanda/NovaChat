add_executable(PresenceService
  main.cpp
  presence_store.cpp
  ${CMAKE_SOURCE_DIR}/proto/common.pb.cc
  ${CMAKE_SOURCE_DIR}/proto/presence.pb.cc
)
target_include_directories(PresenceService PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/proto
  ${NOVA_DEPS_PREFIX}/include
)
target_link_libraries(PresenceService
  rpc muduo_net muduo_base protobuf pthread
)
