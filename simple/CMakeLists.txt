
# ================================================================================================
# exec
# ================================================================================================

add_executable(simple_todo Main.cpp TodoServer.cpp)
target_link_libraries(simple_todo PRIVATE common_poco_config)
