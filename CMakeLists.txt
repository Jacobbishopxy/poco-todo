# @file:	CMakeLists.txt
# @author:	Jacob Xie
# @date:	2024/12/04 09:59:29 Wednesday
# @brief:

cmake_minimum_required(VERSION 3.22)
project(poco_todo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ================================================================================================
# deps
# ================================================================================================

# spdlog
set(SPDLOG_VERSION 1.15.0)
set(SPDLOG_ROOT "/opt/qi-deps/spdlog-${SPDLOG_VERSION}")
message("Using ${SPDLOG_ROOT}")
include_directories("${SPDLOG_ROOT}/include")
link_directories("${SPDLOG_ROOT}/lib")

# poco
set(POCO_VERSION 1.14.0)
set(POCO_ROOT "/opt/qi-deps/poco-${POCO_VERSION}")
message("Using ${POCO_ROOT}")
add_library(common_poco_config INTERFACE)
target_include_directories(common_poco_config INTERFACE "${POCO_ROOT}/include")
target_link_directories(common_poco_config INTERFACE "${POCO_ROOT}/lib")
target_link_libraries(common_poco_config INTERFACE PocoFoundation PocoJSON PocoNet PocoUtil)
# ignore warning from poco package
target_compile_options(common_poco_config INTERFACE -Wno-deprecated-enum-enum-conversion)


# ================================================================================================
# exec
# ================================================================================================

add_subdirectory("simple")
