cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

project(CountVonCount LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(CountVonCount)

# while this is not particularly encouraged, it saves me from reloading CMakeLists all the time
# and as such this is a kind of experiment for me to see if this causes any issues
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

    # MSVC specific, this will avoid showing a console window upon startup
    # (side note, gcc for windows uses '-mwindows': https://gcc.gnu.org/onlinedocs/gcc-4.4.2/gcc/i386-and-x86_002d64-Windows-Options.html
    #target_link_options(CountVonCount PRIVATE /SUBSYSTEM:WINDOWS /entry:mainCRTStartup)
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