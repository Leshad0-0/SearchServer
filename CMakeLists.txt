cmake_minimum_required(VERSION 3.20)
project(search_server)

set(CMAKE_CXX_STANDARD 17)

add_executable(search_server main.cpp document.h document.cpp paginator.h read_input_functions.h read_input_functions.cpp request_queue.h request_queue.cpp search_server.h search_server.cpp string_processing.h string_processing.cpp remove_duplicates.cpp remove_duplicates.h log_duration.h process_queries.cpp process_queries.h concurrent_map.h)
