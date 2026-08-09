cmake_minimum_required(VERSION 3.16)
project(logic-reasoner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(reasoner
    src/main.cpp
    src/reasoner.cpp
    src/json_writer.cpp
)
