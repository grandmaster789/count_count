set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(CountVonCountWasm)
add_library   (CountVonCountLibWasm STATIC)

# ... existing file globbing ...
file(GLOB_RECURSE CountLibSources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl"
)

# Remove native entrypoint and files that depend on native GUI or camera access
list(REMOVE_ITEM CountLibSources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/camera_manager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/main_window_controller.cpp"
)

# WebAssembly-specific configuration

# WebAssembly executable
target_sources(CountVonCountWasm PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/wasm_main.cpp")
target_include_directories(CountVonCountWasm PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(CountVonCountWasm PRIVATE CountVonCountLibWasm)

# WebAssembly library
target_sources(CountVonCountLibWasm PRIVATE ${CountLibSources})
target_include_directories(CountVonCountLibWasm PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Emscripten-specific compile and link options
target_compile_options(CountVonCountWasm PRIVATE
    -sUSE_OPENCV=1
    -O3
)

target_link_options(CountVonCountWasm PRIVATE
    -sUSE_OPENCV=1
    -sWASM=1
    -sEXPORTED_FUNCTIONS=['_main','_process_image_data']
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']
    -sMODULARIZE=1
    -sEXPORT_NAME=CountVonCount
    -sALLOW_MEMORY_GROWTH=1
    --bind
)
