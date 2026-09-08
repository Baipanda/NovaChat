# 命令行客户端（原 ChatClient）
add_executable(NovaClient main.cpp)
target_include_directories(NovaClient PRIVATE
  ${CMAKE_SOURCE_DIR}/gateway/include
  ${CMAKE_SOURCE_DIR}/thirdparty
)
target_link_libraries(NovaClient pthread)
