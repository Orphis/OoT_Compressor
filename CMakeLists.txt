cmake_minimum_required(VERSION 3.0)
project(OoT_Compressor)

set(CMAKE_CXX_STANDARD 14)

add_compile_options($<$<C_COMPILER_ID:Clang>:-fcolor-diagnostics>)
add_compile_options($<$<C_COMPILER_ID:GNU>:-fdiagnostics-color=always>)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

find_package(Threads REQUIRED)

add_library(util STATIC
    crc.cpp
    yaz0.cpp
    yaz0.h
    rom.cpp
    rom.h
    findtable.cpp
    findtable.h
    util.h
)

add_executable(compressor
    compressor.cpp
)
target_link_libraries(compressor
    util
    Threads::Threads
)

add_executable(decompressor
    decompressor.cpp
)
target_link_libraries(decompressor
    util
)

add_executable(table_extractor
    tableExtractor.cpp
)
target_link_libraries(table_extractor
    util
)
