# Standalone Runnable Examples

# 1. EchoBench
add_executable(EchoBench EchoBench/EchoBench.cpp)
target_link_libraries(EchoBench PRIVATE darkfire_net)

# 2. ChatRoom
add_executable(ChatServer ChatRoom/ChatServer.cpp)
target_link_libraries(ChatServer PRIVATE darkfire_net)

add_executable(ChatClient ChatRoom/ChatClient.cpp)
target_link_libraries(ChatClient PRIVATE darkfire_net)

# 3. SpatialReplication
add_executable(SpatialServer SpatialReplication/SpatialServer.cpp)
target_link_libraries(SpatialServer PRIVATE darkfire_net)

add_executable(SpatialClient SpatialReplication/SpatialClient.cpp)
target_link_libraries(SpatialClient PRIVATE darkfire_net)
