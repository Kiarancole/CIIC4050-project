cmake_minimum_required(VERSION 3.10)

project(ground_control)

set(CMAKE_C_STANDARD 11)

add_executable(${CMAKE_PROJECT_NAME}
   src/main.c)

# On Linux, link rt library
if(UNIX AND NOT APPLE)
    target_link_libraries(${CMAKE_PROJECT_NAME} rt)
endif()
