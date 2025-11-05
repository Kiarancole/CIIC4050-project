cmake_minimum_required(VERSION 3.10)

project(air_control_c)

set(CMAKE_C_STANDARD 11)

add_executable(${CMAKE_PROJECT_NAME}
   src/main.c
   src/functions.c)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE include)

# Link pthread library
# On Linux, also link rt library
if(UNIX AND NOT APPLE)
    target_link_libraries(${CMAKE_PROJECT_NAME} pthread rt)
else()
    target_link_libraries(${CMAKE_PROJECT_NAME} pthread)
endif()
