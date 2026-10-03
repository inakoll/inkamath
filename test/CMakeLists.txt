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

# The compiler's C target (DESIGN.md, phase 14, step 2). What it emits
# for each model is the header in compile/expected, and the header it emits,
# built as C, holds the model to exact values its harness knows. The expected
# headers are kept apart, where a harness's #include cannot find one instead.
enable_language(C)
# A model a file defines is compiled by its name, which is the file's.
set(named_models mix)
foreach(model IN ITEMS pid pid_clamped kalman kalman2 fir heat adc net mix loop chain back kernel bank)
    set(header ${CMAKE_CURRENT_BINARY_DIR}/compile/${model}.h)
    set(name "")
    if(model IN_LIST named_models)
        set(name ${model})
    endif()
    add_custom_command(OUTPUT ${header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/compile
        COMMAND inkamath_cli --compile ${CMAKE_CURRENT_SOURCE_DIR}/compile/${model}.ink ${name} -o ${header}
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

# The oracle: an instance compiled, and each of its terms held to the
# interpreter's exact one by the program 'inkamath --check' writes. 'wild'
# drifts from it, and its program says where and exits with a failure.
# Each as file:instance.
foreach(checked IN ITEMS drift:calm drift:wild drift:spin drift:rough drift:brink drift:ledge
                         drift:rift
                         delay:fir newton:stiff logistic:gate softmax:cls steady:mark
                         rnn:rnn attention:head momentum:ball adam:fit lqr:gain)
    string(REPLACE ":" ";" checked ${checked})
    list(GET checked 0 file)
    list(GET checked 1 instance)
    set(program ${CMAKE_CURRENT_BINARY_DIR}/compile/${instance}.c)
    add_custom_command(OUTPUT ${program}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/compile
        COMMAND inkamath_cli --check ${CMAKE_CURRENT_SOURCE_DIR}/compile/${file}.ink ${instance}
                -o ${program}
        DEPENDS inkamath_cli compile/${file}.ink
        VERBATIM)
    add_executable(check_${instance} ${program})
    target_link_libraries(check_${instance} PRIVATE inkamath_warnings
                          $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)
    set_target_properties(check_${instance} PROPERTIES
                          C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    add_test(NAME check_${instance}_report COMMAND check_${instance})
endforeach()
set_tests_properties(check_gate_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^gate: 100 steps from 0, against exact values until 0 and inexact ones from there\n(gate\\.[a-z]+: within [0-9.e+-]+\n)+$")
set_tests_properties(check_cls_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^cls: 100 steps from 0, against exact values until 0 and inexact ones from there\n(cls\\.[a-z]+: within [0-9.e+-]+\n)+$")
set_tests_properties(check_ball_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^ball: 100 steps from 0, against exact values until 0 and inexact ones from there\n(ball\\.[a-z]+: within [0-9.e+-]+\n)+$")
set_tests_properties(check_fit_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^fit: 100 steps from 0, against exact values until 0 and inexact ones from there\n(fit\\.[a-z]+: within [0-9.e+-]+\n)+$")
set_tests_properties(check_gain_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^gain: 100 steps from 0, against exact values\n(gain\\.[A-Za-z]+: within [0-9.e+-]+\n)+$")
set_tests_properties(check_rnn_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^rnn: 100 steps from 0, against exact values until 1 and inexact ones from there\n(rnn\\.[a-z]+: within [0-9.e+-]+\n)+$")
set_tests_properties(check_head_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^head: 100 steps from 0, against exact values until 0 and inexact ones from there\n(head\\.[A-Za-z]+: within [0-9.e+-]+\n)+$")
set_tests_properties(check_mark_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^mark: 100 steps from 0, against exact values until 0 and inexact ones from there\n(mark\\.[a-z]+: within [0-9.e+-]+\n)+$")
set_tests_properties(check_stiff_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^stiff: 100 steps from 0, against exact values until 0 and inexact ones from there\n(stiff\\.[a-z]+: within [0-9.e+-]+\n)+$")
set_tests_properties(check_fir_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^fir: 100 steps from 0, against exact values\n(fir\\.[a-z]+: within 0\n)+$")
set_tests_properties(check_calm_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^calm: 100 steps from 0, against exact values\ncalm\\.v: within [0-9.e+-]+\n$")
set_tests_properties(check_wild_report PROPERTIES PASS_REGULAR_EXPRESSION
    "wild\\.d: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\n$")
set_tests_properties(check_spin_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^spin: 100 steps from 0, against exact values\nspin\\.p: within [0-9.e+-]+\n$")
set_tests_properties(check_rough_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^rough: 100 steps from 0, against exact values until 1 and inexact ones from there\nrough\\.v: within [0-9.e+-]+\n$")
set_tests_properties(check_brink_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^brink: 100 steps from 0, against exact values\nbrink\\.g: at 1 the compiled step takes 'g_n \\| d_n < c - w = 1' and the interpreter 'g_n = 0'; the guard of the first is exactly on its threshold\nbrink\\.d: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\nbrink\\.g: 1 at 1, where the interpreter gives 0\n$")
set_tests_properties(check_ledge_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^ledge: 100 steps from 0, against exact values\nledge\\.g: at 6 the compiled step takes 'g_n \\| d_n < c - w = 1' and the interpreter 'g_n = 0'; the guard of the first is 1e-12 from its threshold\n")
set_tests_properties(check_rift_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^rift: 100 steps from 0, against exact values\nrift\\.g\\[1,1\\]: at 1 the compiled step takes 'g_n\\[j<=2\\] \\| d_n\\[j\\] < c\\*j = 1' and the interpreter 'g_n\\[j<=2\\] = 0'; the guard of the first is exactly on its threshold\nrift\\.d\\[1,1\\]: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\nrift\\.g\\[1,1\\]: 1 at 1, where the interpreter gives 0\n$")
add_test(NAME check_calm COMMAND check_calm)
add_test(NAME check_wild COMMAND check_wild)
add_test(NAME check_brink COMMAND check_brink)
add_test(NAME check_rift COMMAND check_rift)
set_tests_properties(check_wild check_brink check_rift PROPERTIES WILL_FAIL ON)

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
