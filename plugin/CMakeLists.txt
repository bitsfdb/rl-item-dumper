cmake_minimum_required(VERSION 3.20)
project(ItemDumper CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# BakkesMod SDK — uses %APPDATA% env var at configure time
set(BMSDK "$ENV{APPDATA}/bakkesmod/bakkesmod/bakkesmodsdk")

add_library(ItemDumper SHARED
    pch.cpp
    ItemDumper.cpp
    RLPathFinder.cpp
    UPKReader.cpp
    TextureExtractor.cpp
    JsonBuilder.cpp
    third_party/miniz.c
    third_party/miniz_tdef.c
    third_party/miniz_tinfl.c
    third_party/miniz_zip.c
)

# Force miniz C files to compile as C (not C++) to avoid name mangling
# and exclude them from the PCH (PCH is C++ only)
set_source_files_properties(
    third_party/miniz.c
    third_party/miniz_tdef.c
    third_party/miniz_tinfl.c
    third_party/miniz_zip.c
    PROPERTIES
        LANGUAGE C
        SKIP_PRECOMPILE_HEADERS ON
)

target_precompile_headers(ItemDumper PRIVATE pch.h)

target_include_directories(ItemDumper PRIVATE
    "${BMSDK}/include"
    "third_party"
    "third_party/nlohmann"
    "."
)

target_link_directories(ItemDumper PRIVATE "${BMSDK}/lib")

target_link_libraries(ItemDumper PRIVATE
    pluginsdk
    BCrypt
    Advapi32
)

target_compile_definitions(ItemDumper PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WINDLL
    _CRT_SECURE_NO_WARNINGS
)

# Copy DLL to BakkesMod plugins folder after build
add_custom_command(TARGET ItemDumper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:ItemDumper>"
        "$ENV{APPDATA}/bakkesmod/bakkesmod/plugins/ItemDumper.dll"
    COMMENT "Deploying ItemDumper.dll to BakkesMod plugins"
)
