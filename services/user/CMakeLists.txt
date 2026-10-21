add_executable(UserService
  main.cpp
  ${CMAKE_SOURCE_DIR}/gateway/src/db/db.cpp
  ${CMAKE_SOURCE_DIR}/gateway/src/model/usermodel.cpp
  ${CMAKE_SOURCE_DIR}/proto/common.pb.cc
  ${CMAKE_SOURCE_DIR}/proto/user.pb.cc
)

target_include_directories(UserService PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/proto
  ${CMAKE_SOURCE_DIR}/gateway/include
  ${CMAKE_SOURCE_DIR}/gateway/include/db
  ${CMAKE_SOURCE_DIR}/gateway/include/model
  ${NOVA_DEPS_PREFIX}/include
)

target_link_libraries(UserService
  rpc
  muduo_net muduo_base
  mysqlclient
  protobuf
  zookeeper_mt
  pthread
)
