cmake_minimum_required(VERSION 3.29)
project(NeuralAutomata C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")


add_executable(NeuralAutomata main.c)

find_package(SDL3 REQUIRED)
target_link_libraries(NeuralAutomata SDL3)
target_link_libraries(NeuralAutomata m)