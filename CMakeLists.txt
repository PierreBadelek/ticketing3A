cmake_minimum_required(VERSION 3.10)
project(ticketing3A C)

set(CMAKE_C_STANDARD 11)

# Dossier des headers
include_directories(include)

# Serveur
add_executable(server
        src/server.c
        src/server_functions.c
)

# Client
add_executable(client
        src/client.c
)

# Librairies nécessaires
target_link_libraries(server pthread rt)
target_link_libraries(client pthread rt)