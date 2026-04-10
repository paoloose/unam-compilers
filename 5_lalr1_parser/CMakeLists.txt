cmake_minimum_required(VERSION 3.16)
project(first_and_follow C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(FLEX REQUIRED)
flex_target(generate_scanner
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scanner.l
    ${CMAKE_CURRENT_BINARY_DIR}/scanner.c
)

add_executable(first_and_follow
    ./src/main.c
    ./src/grammar.c
    ./src/analyzer.c
    ./src/automaton.c
    ./src/parser.c
    ${FLEX_generate_scanner_OUTPUTS}
)

target_include_directories(first_and_follow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)