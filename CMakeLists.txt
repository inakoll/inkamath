cmake_minimum_required(VERSION 3.20)

project(inkamath
    VERSION 1.0.0
    DESCRIPTION "A small mathematical expression interpreter"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(INKAMATH_BUILD_TESTS "Build the test suite" ON)
option(INKAMATH_WERROR     "Treat compiler warnings as errors" OFF)
set(INKAMATH_SANITIZE "" CACHE STRING "Sanitizers to enable, e.g. address,undefined")

# --- warning set -------------------------------------------------------------
# Kept as an INTERFACE target so it is opt-in per target and never leaks to
# vendored third-party code.
add_library(inkamath_warnings INTERFACE)
target_compile_options(inkamath_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)
# MSVC deprecates standard functions such as getenv in favour of its own _s
# variants, which are not standard C++.
target_compile_definitions(inkamath_warnings INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
if(INKAMATH_WERROR)
    target_compile_options(inkamath_warnings INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>)
endif()

if(INKAMATH_SANITIZE)
    add_compile_options(-fsanitize=${INKAMATH_SANITIZE} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${INKAMATH_SANITIZE})
    # -O0 makes the sanitized test suite several times slower for no
    # check gained; -Og keeps most of -O1's speed at half its compile time.
    add_compile_options($<$<CONFIG:Debug>:-Og>)
    # Container precondition checks. The sanitizers miss an out-of-range
    # operator[] whenever the index still lands inside the allocation, which
    # is most of the time for a vector with spare capacity.
    add_compile_definitions(_GLIBCXX_ASSERTIONS)
endif()

# The token limit keeps a line's recursion off the end of the stack, and it
# was measured against the 8 MB Linux gives the main thread (interpreter.hpp,
# max_tokens). Windows gives 1 MB, and there a Debug build overflowed on a line
# the limit accepts (DESIGN.md, C63).
if(MSVC)
    add_link_options(/STACK:8388608)
    # The interpreter is one translation unit of templates, and a Debug build
    # of it passed the sections an object file holds without /bigobj.
    add_compile_options(/bigobj)
endif()

# --- the interpreter ---------------------------------------------------------
add_library(inkamath INTERFACE)
target_include_directories(inkamath INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# The prelude's compiled functions round as its definitions do, which a fused
# multiply-add, contracted where the target has one, does not (DESIGN.md).
target_compile_options(inkamath INTERFACE
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-ffp-contract=off>)

# --- the REPL ----------------------------------------------------------------
add_executable(inkamath_cli src/main.cpp)
target_link_libraries(inkamath_cli PRIVATE inkamath inkamath_warnings)
set_target_properties(inkamath_cli PROPERTIES OUTPUT_NAME inkamath)
# The version is written here and nowhere else.
target_compile_definitions(inkamath_cli PRIVATE INKAMATH_VERSION="${PROJECT_VERSION}")

# Not a test: a benchmark informs a change and never gates one.
add_custom_target(bench
    COMMAND ${CMAKE_COMMAND} -DINKAMATH=$<TARGET_FILE:inkamath_cli>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/bench/run.cmake
    DEPENDS inkamath_cli
    USES_TERMINAL VERBATIM)

# --- tests -------------------------------------------------------------------
if(INKAMATH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
