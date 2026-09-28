add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/third_party)

add_executable(inkamath_tests
    bignum_test.cpp
    doctest_main.cpp
    interpreter_test.cpp
    line_editor_test.cpp
    matrix_test.cpp
    )

target_link_libraries(inkamath_tests PRIVATE inkamath doctest inkamath_warnings)
target_include_directories(inkamath_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(inkamath_tests PRIVATE
    INKAMATH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    INKAMATH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# One ctest entry per doctest suite, so that a crash in one reports as a
# single failure rather than taking the others' results with it.
foreach(suite IN ITEMS bignum interpreter line_editor matrix)
    add_test(NAME ${suite} COMMAND inkamath_tests --test-suite=${suite} --no-intro)
    set_tests_properties(${suite} PROPERTIES TIMEOUT 60)
endforeach()

add_test(NAME cli
         COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:inkamath_cli>
                 -DOUT=${CMAKE_CURRENT_BINARY_DIR}/cli
                 -P ${CMAKE_CURRENT_LIST_DIR}/cli.cmake)
set_tests_properties(cli PROPERTIES TIMEOUT 120)

# Regenerate the golden transcripts from current behaviour. Always review the
# resulting diff: it is the record of what the change you just made did.
add_custom_target(record_goldens
    COMMAND ${CMAKE_COMMAND} -E env INKAMATH_RECORD=1
            $<TARGET_FILE:inkamath_tests> --test-suite=interpreter
    DEPENDS inkamath_tests
    VERBATIM)

# The compiler's C target (MODERNIZATION.md, phase 14, step 2). What it emits
# for each model is the header in compile/expected, and the header it emits,
# built as C, holds the model to exact values its harness knows. The expected
# headers are kept apart, where a harness's #include cannot find one instead.
enable_language(C)
foreach(model IN ITEMS pid pid_clamped kalman kalman2 fir heat)
    set(header ${CMAKE_CURRENT_BINARY_DIR}/compile/${model}.h)
    add_custom_command(OUTPUT ${header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/compile
        COMMAND inkamath_cli --compile ${CMAKE_CURRENT_SOURCE_DIR}/compile/${model}.ink -o ${header}
        DEPENDS inkamath_cli compile/${model}.ink
        VERBATIM)
    add_executable(compile_${model} compile/${model}_test.c ${header})
    target_include_directories(compile_${model} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/compile)
    target_link_libraries(compile_${model} PRIVATE inkamath_warnings
                          $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)
    set_target_properties(compile_${model} PROPERTIES
                          C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    add_test(NAME compile_${model} COMMAND compile_${model})
    add_test(NAME compile_${model}_header
             COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol
                     ${CMAKE_CURRENT_SOURCE_DIR}/compile/expected/${model}.h ${header})
endforeach()

# Random models, compiled and held to the interpreter's answers: the generator
# runs both at build time and writes one C file, built and run like the others.
add_executable(compile_fuzz_generator compile/fuzz.cpp)
target_link_libraries(compile_fuzz_generator PRIVATE inkamath inkamath_warnings)
set(fuzz_source ${CMAKE_CURRENT_BINARY_DIR}/compile/fuzz.c)
add_custom_command(OUTPUT ${fuzz_source}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/compile
    COMMAND compile_fuzz_generator ${fuzz_source}
    DEPENDS compile_fuzz_generator
    VERBATIM)
add_executable(compile_fuzz ${fuzz_source})
target_link_libraries(compile_fuzz PRIVATE inkamath_warnings $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)
set_target_properties(compile_fuzz PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME compile_fuzz COMMAND compile_fuzz)
