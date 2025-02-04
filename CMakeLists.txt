cmake_minimum_required(VERSION 3.3.0)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

project(CountVonCount LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(CountVonCount)

file(GLOB_RECURSE CountSources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl"
)

target_sources            (CountVonCount PRIVATE ${CountSources})
target_include_directories(CountVonCount PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries     (CountVonCount PRIVATE
        ${OpenCV_LIBS}
)
target_link_directories   (CountVonCount PRIVATE
        ${OpenCV_LIB_DIR}
)

if (MSVC)
    target_compile_options(CountVonCount PRIVATE
            /W4 # Enable all warnings
            /WX # Warnings as errors
            /MP # Enable multi-processor compilation
    )
else()
    # technically these are for gcc and clang only
    target_compile_options(CountVonCount PRIVATE
            -Wall      # Enable all warnings
            -Wextra    # Enable extra warnings
            -Werror    # Warnings as errors
            -Wpedantic # Enable pedantic warnings
    )
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CountVonCount) # for when using visual studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)