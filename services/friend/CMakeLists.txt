add_executable(FriendService main.cpp)
target_link_libraries(FriendService mprpc muduo_net muduo_base protobuf pthread)
