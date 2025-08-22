cmake_minimum_required(VERSION 3.10)

if(EMSCRIPTEN)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" CACHE STRING "Emscirpten toolchain")
else()
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

project(CountVonCount LANGUAGES CXX VERSION 0.0.1)

if (EMSCRIPTEN)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/wasm_build.cmake")
else()
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/native_build.cmake")
endif()
