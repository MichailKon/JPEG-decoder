cmake_minimum_required(VERSION 3.21)
project(jpeg-decoder VERSION 1.0.0 DESCRIPTION "my jpeg decoder")

set(CMAKE_CXX_STANDARD 20)

find_package(FFTW REQUIRED)

file(GLOB sources "sources/*.cpp")
file(GLOB include "include/*.h")

include_directories(include)

add_executable(${PROJECT_NAME} main.cpp ${sources})
