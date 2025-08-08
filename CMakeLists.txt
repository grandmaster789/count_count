cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

project(CountVonCount LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(Stb REQUIRED)
find_package(Catch2 3 REQUIRED)

add_executable(CountVonCount)       # main executable
add_library   (CountVonCountLib)    # project code as a static library
add_executable(CountVonCountTests)  # unit tests

# while this is not particularly encouraged, it saves me from reloading CMakeLists all the time
# and as such this is a kind of experiment for me to see if this causes any issues
file(GLOB_RECURSE CountLibSources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl"
)

# main.cpp is the only file for the application entry point
list(REMOVE_ITEM CountLibSources "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

file(GLOB_RECURSE CountUnitTestSources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.inl"
)

# ----------- Main executable -------------
target_sources            (CountVonCount PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
target_include_directories(CountVonCount PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        ${OpenCV_INCLUDE_DIRS}
        ${Stb_INCLUDE_DIR}
)
target_link_libraries     (CountVonCount PRIVATE CountVonCountLib
        ${OpenCV_LIBS}
)

# ----------- Project code -----------------
target_sources            (CountVonCountLib PRIVATE ${CountLibSources})
target_include_directories(CountVonCountLib PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        ${OpenCV_INCLUDE_DIRS}
        ${Stb_INCLUDE_DIR}
)
target_link_libraries     (CountVonCountLib PRIVATE
        ${OpenCV_LIBS}
)
target_link_directories   (CountVonCountLib PRIVATE
        ${OpenCV_LIB_DIR}
)

# ----------- Test code --------------------
target_sources            (CountVonCountTests PRIVATE ${CountUnitTestSources})
target_include_directories(CountVonCountTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
        ${OpenCV_INCLUDE_DIRS}
        ${Stb_INCLUDE_DIR}
)
target_link_libraries     (CountVonCountTests PRIVATE
        ${OpenCV_LIBS}
        Catch2::Catch2WithMain
        CountVonCountLib
)
target_link_directories   (CountVonCountTests PRIVATE
        ${OpenCV_LIB_DIR}
)


if (MSVC)
    target_compile_definitions(CountVonCountLib   PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(CountVonCountTests PRIVATE _CRT_SECURE_NO_WARNINGS)

    target_compile_options(CountVonCountLib PRIVATE
            /W4 # Enable all warnings
            /WX # Warnings as errors
            /MP # Enable multi-processor compilation
    )

    target_compile_options(CountVonCountTests PRIVATE
            /W4 # Enable all warnings
            /WX # Warnings as errors
            /MP # Enable multi-processor compilation
    )

    # MSVC specific, this will avoid showing a console window upon startup
    # (side note, gcc for windows uses '-mwindows': https://gcc.gnu.org/onlinedocs/gcc-4.4.2/gcc/i386-and-x86_002d64-Windows-Options.html
    #target_link_options(CountVonCount PRIVATE /SUBSYSTEM:WINDOWS /entry:mainCRTStartup)
else()
    # technically these are for gcc and clang only
    target_compile_options(CountVonCountLib PRIVATE
            -Wall      # Enable all warnings
            -Wextra    # Enable extra warnings
            -Werror    # Warnings as errors
            -Wpedantic # Enable pedantic warnings
    )

    target_compile_options(CountVonCountTests PRIVATE
            -Wall      # Enable all warnings
            -Wextra    # Enable extra warnings
            -Werror    # Warnings as errors
            -Wpedantic # Enable pedantic warnings
    )
endif()

enable_testing()
add_test(NAME CountVonCountUnitTests COMMAND CountVonCountTests)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CountVonCount) # for when using visual studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
