cmake_minimum_required(VERSION 4.0)
project(ticketing3A C)

set(CMAKE_C_STANDARD 11)

add_executable(ticketing3A
        client/main.c
        server/main.c)
