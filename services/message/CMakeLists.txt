add_executable(MessageService main.cpp)
target_link_libraries(MessageService mprpc muduo_net muduo_base protobuf pthread)
