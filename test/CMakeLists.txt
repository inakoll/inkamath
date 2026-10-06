# The checks hold a compiled step to the interpreter's rounding, which a fused
# multiply-add, contracted where the target has one, does not keep (C97).
add_compile_options(
    $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:GNU,Clang,AppleClang>>:-ffp-contract=off>)

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

# 2^k as a step computes it, exact for every k the prelude scales by.
add_executable(compile_powers compile/powers.c)
target_link_libraries(compile_powers PRIVATE inkamath_warnings
                      $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)
set_target_properties(compile_powers PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME compile_powers COMMAND compile_powers)

# The prelude's functions the interpreter calls on a double (DESIGN.md),
# checked in so that building the interpreter needs no interpreter; recorded
# as the goldens are, and its diff read.
set(prelude ${PROJECT_SOURCE_DIR}/include/inkamath/inkamath_prelude)
set(prelude_written ${CMAKE_CURRENT_BINARY_DIR}/prelude/inkamath_prelude.h)
add_custom_target(record_prelude
    COMMAND inkamath_cli --compile ${prelude}.ink -o ${prelude}.h
    DEPENDS inkamath_cli VERBATIM)
add_custom_command(OUTPUT ${prelude_written}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/prelude
    COMMAND inkamath_cli --compile ${prelude}.ink -o ${prelude_written}
    DEPENDS inkamath_cli ${prelude}.ink VERBATIM)
add_custom_target(prelude_header_written ALL DEPENDS ${prelude_written})
add_test(NAME prelude_header
         COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol ${prelude}.h ${prelude_written})

# The oracle: an instance compiled, and each of its terms held to the
# interpreter's exact one by the program 'inkamath --check' writes. 'wild'
# drifts from it, and its program says where and exits with a failure.
# Each as file:instance.
foreach(checked IN ITEMS drift:calm drift:wild drift:spin drift:rough drift:brink drift:ledge
                         drift:rift drift:naught
                         delay:fir newton:stiff logistic:gate sized:sized softmax:cls steady:mark
                         rnn:rnn attention:head momentum:ball adam:fit lqr:gain
                         rates:pairs rates:odd seeded:warm cascade:servo decimate:boxcar
                         decimate:stride decimate:frame decimate:lead
                         decimate:trail drift:sill drift:seam history:past history:twin
                         history:lit history:rise history:ajar history:sown history:nest history:kin history:vast
                         history:twice history:relayed history:pole
                         history:huge history:veiled history:cloaked history:two history:masked
                         nan:run nan:sign nan:shy nan:slope nan:pair
                         nan:lap nan:trim nan:any nan:cell nan:held inputs:fan inputs:grid
                         inputs:chan inputs:twirl inputs:duo inputs:mean3 inputs:mean2
                         inputs:track inputs:twain inputs:nestv inputs:fed estimate:tie
                         estimate:chaos estimate:dyad estimate:flat estimate:halved
                         estimate:steep estimate:giant elementary:els)
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
# The interpreter's own error, estimated where any term is inexact (DESIGN.md):
# its digits are the seeds', so held by its form but where a limit's
# remainder alone gives them.
set(about "\\; the interpreter's terms about [0-9.e+-]+ from the exact ones")
set_tests_properties(check_tie_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^tie: 100 steps from 0, against exact values until 0 and inexact ones from there\ntie\\.y: within 0\\; the interpreter's terms about 5\\.8e-11 from the exact ones\ntie\\.g: within 0\\; the interpreter's terms about 1 from the exact ones, past the tolerance from 0\n$")
set_tests_properties(check_chaos_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^chaos: 100 steps from 0, against exact values until 0 and inexact ones from there\nchaos\\.x: within 0${about}, past the tolerance from [0-9]+\n$")
set_tests_properties(check_dyad_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^dyad: 100 steps from 0, against exact values until 0 and inexact ones from there\ndyad\\.x: within 0${about}, past the tolerance from [0-9]+\n$")
set_tests_properties(check_flat_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^flat: 100 steps from 0, against exact values until 0 and inexact ones from there\nflat\\.r: within 0\nflat\\.g: within 0\n$")
set_tests_properties(check_halved_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^halved: 100 steps from 0, against exact values until 0 and inexact ones from there\nhalved\\.x: within 0\nhalved\\.y: within 0\n$")
set_tests_properties(check_steep_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^steep: 100 steps from 0, against exact values until 0 and inexact ones from there\nsteep\\.y: within 0\\; the interpreter's terms about [1-9](\\.[0-9])?e-1[01] from the exact ones\n$")
set_tests_properties(check_giant_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^giant: 100 steps from 0, against exact values until 0 and inexact ones from there\ngiant\\.y: within 0\\; the interpreter's terms about [1-9](\\.[0-9])?e\\+186 from the exact ones\n$")
# The logistic model's four sigmoids are one limit, compiled once.
add_test(NAME check_gate_one_limit
         COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/compile/gate.c)
set_tests_properties(check_gate_one_limit PROPERTIES
                     PASS_REGULAR_EXPRESSION "gate_lim0\\(" FAIL_REGULAR_EXPRESSION "gate_lim1\\(")
set_tests_properties(check_gate_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^gate: 100 steps from 0, against exact values until 0 and inexact ones from there\n(gate\\.[a-z]+: within [0-9.e+-]+${about}\n)+$")
# exp, log and tanh as the prelude writes them, each a function of the
# header's own, compiled once (DESIGN.md): every term the interpreter's.
set_tests_properties(check_els_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^els: 100 steps from 0, against exact values until 0 and inexact ones from there\nels\\.e: within 0${about}\nels\\.g: within 0${about}\nels\\.h: within 0${about}\nels\\.l: within 0${about}\nels\\.t: within 0${about}\n$")
add_test(NAME check_els_functions
         COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/compile/els.c)
set_tests_properties(check_els_functions PROPERTIES
    PASS_REGULAR_EXPRESSION "els_exp\\(double arg_x\\).*els_log\\(double arg_x\\).*els_tanh\\(double arg_x\\)"
    FAIL_REGULAR_EXPRESSION "pow\\(2\\.718")
set_tests_properties(check_sized_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^sized: 100 steps from 0, against exact values until 0 and inexact ones from there\n(sized\\.[a-z]+: within [0-9.e+-]+${about}\n)+$")
# Its sigmoid's size inferred, compiled where it is called, gives the header
# its bounds written give, byte for byte (DESIGN.md, sizes inferred).
file(READ compile/sized.ink sized_inferred)
string(REPLACE "sig(z)[i,j]" "sig(z)[i<=4, j<=1]" sized_bounded "${sized_inferred}")
if(sized_bounded STREQUAL sized_inferred)
    message(FATAL_ERROR "compile/sized.ink no longer defines sig(z)[i,j]")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS compile/sized.ink)
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/compile/bounded/sized.ink
     CONTENT "${sized_bounded}" @ONLY)
foreach(variant IN ITEMS inferred bounded)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/compile/sized.ink)
    if(variant STREQUAL bounded)
        set(source ${CMAKE_CURRENT_BINARY_DIR}/compile/bounded/sized.ink)
    endif()
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/compile/${variant}/sized.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/compile/${variant}
        COMMAND inkamath_cli --compile ${source} -o ${CMAKE_CURRENT_BINARY_DIR}/compile/${variant}/sized.h
        DEPENDS inkamath_cli ${source}
        VERBATIM)
endforeach()
add_custom_target(sized_headers ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/compile/inferred/sized.h
                                            ${CMAKE_CURRENT_BINARY_DIR}/compile/bounded/sized.h)
add_test(NAME compile_sized_header
         COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_CURRENT_BINARY_DIR}/compile/inferred/sized.h
                 ${CMAKE_CURRENT_BINARY_DIR}/compile/bounded/sized.h)
set_tests_properties(check_cls_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^cls: 100 steps from 0, against exact values until 0 and inexact ones from there\ncls\\.z: within 0\n(cls\\.[a-z]+: within [0-9.e+-]+${about}\n)+$")
set_tests_properties(check_ball_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^ball: 100 steps from 0, against exact values until 0 and inexact ones from there\n(ball\\.[a-z]+: within [0-9.e+-]+${about}\n)+$")
set_tests_properties(check_fit_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^fit: 100 steps from 0, against exact values until 0 and inexact ones from there\n(fit\\.[a-z]+: within [0-9.e+-]+${about}\n)+$")
set_tests_properties(check_gain_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^gain: 100 steps from 0, against exact values\ngain\\.K: within [0-9.e+-]+, from 1\ngain\\.P: within [0-9.e+-]+\n$")
set_tests_properties(check_pairs_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^pairs: 100 steps from 0, against exact values\npairs\\.y: within 0\npairs\\.w: within 0, from 2\npairs\\.z: within 0\n$")
set_tests_properties(check_odd_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^odd: 100 steps from 1, against exact values\nodd\\.y: within 0\nodd\\.z: within 0, from 2\n$")
set_tests_properties(check_servo_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^servo: 100 steps from 0, against exact values\n(servo\\.[a-z]+: within [0-9.e+-]+\n)+$")
# A NaN reaches every term that reads it (DESIGN.md): each guard, comparison
# and power reading a failure answers NaN, where the interpreter gives none.
set_tests_properties(check_run_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^run: 100 steps from 0, against exact values until 0 and inexact ones from there\nrun\\.y: within [0-9.e+-]+\\; the interpreter's terms about 9\\.4e-11 from the exact ones\nrun\\.high: within 0\n$")
set_tests_properties(check_sign_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^sign: 100 steps from 0, against exact values\nsign\\.y: within 0\nsign\\.on: within 0\nsign\\.p: within 0\n$")
set_tests_properties(check_shy_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^shy: 100 steps from 0, against exact values\nshy\\.y: within 0\nshy\\.z: within 0\n$")
set_tests_properties(check_slope_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^slope: 100 steps from 0, against exact values\nslope\\.y: within 0\nslope\\.g: within 0\nslope\\.h: within 0\n$")
set_tests_properties(check_pair_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^pair: 100 steps from 0, against exact values\npair\\.w: within 0\npair\\.y: within 0\npair\\.z: within 0\n$")
set_tests_properties(check_lap_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^lap: 100 steps from 0, against exact values until 0 and inexact ones from there\nlap\\.y: within [0-9.e+-]+\\; the interpreter's terms about 5\\.8e-11 from the exact ones\n$")
set_tests_properties(check_trim_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^trim: 100 steps from 0, against exact values\ntrim\\.y: within 0\n$")
set_tests_properties(check_any_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^any: 100 steps from 0, against exact values\nany\\.y: within 0\nany\\.o: within 0\n$")
set_tests_properties(check_cell_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^cell: 100 steps from 0, against exact values\ncell\\.b: within 0\ncell\\.c: within 0\ncell\\.m: within 0\ncell\\.p: within 0\n$")
set_tests_properties(check_held_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^held: 100 steps from 0, against exact values\nheld\\.d: within 0\n$")
# A model's input of more than one cell (DESIGN.md, C83), every term exact.
set_tests_properties(check_fan_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^fan: 100 steps from 0, against exact values\nfan\\.y: within 0\n$")
set_tests_properties(check_grid_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^grid: 100 steps from 0, against exact values\ngrid\\.d: within 0\ngrid\\.t: within 0\n$")
set_tests_properties(check_chan_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^chan: 100 steps from 0, against exact values\nchan\\.g: within 0\nchan\\.h: within 0\nchan\\.s: within 0\n$")
set_tests_properties(check_twirl_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^twirl: 100 steps from 0, against exact values\ntwirl\\.c: within 0\n$")
set_tests_properties(check_duo_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^duo: 100 steps from 0, against exact values\nduo\\.f: within 0\nduo\\.p: within 0\nduo\\.q: within 0\nduo\\.y: within 0, from 1\nduo\\.z: within 0\n$")
set_tests_properties(check_mean3_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^mean3: 100 steps from 0, against exact values\nmean3\\.y: within 0\n$")
set_tests_properties(check_mean2_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^mean2: 100 steps from 0, against exact values\nmean2\\.y: within 0\n$")
set_tests_properties(check_track_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^track: 100 steps from 0, against exact values\ntrack\\.u: within 0\n$")
set_tests_properties(check_twain_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^twain: 100 steps from 0, against exact values\ntwain\\.y: within 0\n$")
set_tests_properties(check_nestv_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^nestv: 100 steps from 0, against exact values\nnestv\\.inner\\.x: within 0\nnestv\\.inner\\.y: within 0\nnestv\\.y: within 0\n$")
set_tests_properties(check_boxcar_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^boxcar: 100 steps from 0, against exact values\nboxcar\\.f: within 0\nboxcar\\.t: within 0\nboxcar\\.y: within 0, from 1\nboxcar\\.w: within 0\nboxcar\\.z: within 0\nboxcar\\.q: within 0, from 3\n$")
set_tests_properties(check_stride_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^stride: 100 steps from 0, against exact values\nstride\\.h: within 0, from 1\n$")
set_tests_properties(check_frame_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^frame: 100 steps from 0, against exact values\nframe\\.p: within 0, from 2\n$")
set_tests_properties(check_lead_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^lead: 100 steps from 0, against exact values\nlead\\.y: within 0\n$")
set_tests_properties(check_trail_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^trail: 100 steps from 0, against exact values\ntrail\\.y: within 0\ntrail\\.w: within 0, from 1\ntrail\\.z: within 0\n$")
set_tests_properties(check_past_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^past: 100 steps from 0, against exact values\npast\\.c: within 0\n$")
set_tests_properties(check_twin_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^twin: 100 steps from 0, against exact values\ntwin\\.c: within 0\n$")
set_tests_properties(check_lit_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^lit: 100 steps from 0, against exact values\nlit\\.f: within 0\nlit\\.y: within 0, from 1\nlit\\.z: within 0\n$")
set_tests_properties(check_rise_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^rise: 100 steps from 0, against exact values\nrise\\.c: within 0\n$")
set_tests_properties(check_ajar_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^ajar: 100 steps from 0, against exact values\najar\\.c: within 0\n$")
set_tests_properties(check_sown_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^sown: 100 steps from 0, against exact values\nsown\\.y: within 0\nsown\\.z: within 0\n$")
set_tests_properties(check_nest_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^nest: 100 steps from 0, against exact values\nnest\\.inner\\.c: within 0\nnest\\.c: within 0\nnest\\.inner\\.u: within 0\n$")
set_tests_properties(check_twice_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^twice: 100 steps from 0, against exact values\ntwice\\.inner\\.c: within 0\ntwice\\.c: within 0\ntwice\\.inner\\.x: within 0\n$")
set_tests_properties(check_relayed_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^relayed: 100 steps from 0, against exact values\nrelayed\\.inner\\.c: within 0\nrelayed\\.c: within 0\nrelayed\\.y: within 0\nrelayed\\.inner\\.x: within 0\n$")
set_tests_properties(check_veiled_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^veiled: 100 steps from 0, against exact values\nveiled\\.c_bare\\.c: within 0\nveiled\\.c: within 0\nveiled\\.c_bare\\.u: within 0\n$")
set_tests_properties(check_cloaked_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^cloaked: 100 steps from 0, against exact values\ncloaked\\.inner\\.c_bare\\.c: within 0\ncloaked\\.inner\\.c: within 0\ncloaked\\.d: within 0\ncloaked\\.inner\\.x: within 0\ncloaked\\.inner\\.c_bare\\.u: within 0\n$")
set_tests_properties(check_two_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^two: 100 steps from 0, against exact values\ntwo\\.a\\.c_bare\\.c: within 0\ntwo\\.a\\.c: within 0\ntwo\\.a\\.x: within 0\ntwo\\.a\\.c_bare\\.u: within 0\ntwo\\.b\\.c_bare\\.c: within 0\ntwo\\.b\\.c: within 0\ntwo\\.b\\.x: within 0\ntwo\\.b\\.c_bare\\.u: within 0\ntwo\\.d: within 0\n$")
set_tests_properties(check_masked_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^masked: 100 steps from 0, against exact values\nmasked\\.e_tenth\\.d: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\nmasked\\.e: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\n$")
set_tests_properties(check_pole_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^pole: 100 steps from 0, against exact values\npole\\.y: inf at 3, where the interpreter gives none: division by zero\n$")
set_tests_properties(check_huge_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^huge: 100 steps from 0, against exact values\nhuge\\.y: inf at 31, where the interpreter's term is too large for a double\n$")
set_tests_properties(check_warm_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^warm: 100 steps from 0, against exact values\n(warm\\.[a-z]+: within [0-9.e+-]+\n)+$")
set_tests_properties(check_rnn_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^rnn: 100 steps from 0, against exact values until 1 and inexact ones from there\n(rnn\\.[a-z]+: within [0-9.e+-]+${about}\n)+$")
set_tests_properties(check_head_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^head: 100 steps from 0, against exact values until 0 and inexact ones from there\nhead\\.K: within 0\nhead\\.Q: within 0\nhead\\.S: within [0-9.e+-]+${about}\nhead\\.A: within [0-9.e+-]+${about}\nhead\\.V: within 0\nhead\\.O: within [0-9.e+-]+${about}\n$")
set_tests_properties(check_mark_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^mark: 100 steps from 0, against exact values until 0 and inexact ones from there\n(mark\\.[a-z]+: within [0-9.e+-]+${about}\n)+$")
set_tests_properties(check_stiff_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^stiff: 100 steps from 0, against exact values until 0 and inexact ones from there\nstiff\\.h: within 0\n(stiff\\.[a-z]+: within [0-9.e+-]+${about}\n)+$")
set_tests_properties(check_fir_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^fir: 100 steps from 0, against exact values\n(fir\\.[a-z]+: within 0\n)+$")
set_tests_properties(check_naught_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^naught: 100 steps from 0, against exact values\nnaught\\.y: within 0\n$")
set_tests_properties(check_calm_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^calm: 100 steps from 0, against exact values\ncalm\\.v: within [0-9.e+-]+\n$")
set_tests_properties(check_wild_report PROPERTIES PASS_REGULAR_EXPRESSION
    "wild\\.d: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\n$")
set_tests_properties(check_spin_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^spin: 100 steps from 0, against exact values\nspin\\.p: within [0-9.e+-]+\n$")
set_tests_properties(check_rough_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^rough: 100 steps from 0, against exact values until 1 and inexact ones from there\nrough\\.v: within [0-9.e+-]+${about}\n$")
set_tests_properties(check_brink_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^brink: 100 steps from 0, against exact values\nbrink\\.g: at 1 the compiled step takes 'g_n \\| d_n < c - w = 1' and the interpreter 'g_n = 0'\\; the guard of the first is exactly on its threshold\nbrink\\.d: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\nbrink\\.g: 1 at 1, where the interpreter gives 0\n$")
set_tests_properties(check_ledge_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^ledge: 100 steps from 0, against exact values\nledge\\.g: at 6 the compiled step takes 'g_n \\| d_n < c - w = 1' and the interpreter 'g_n = 0'\\; the guard of the first is 1e-12 from its threshold\n")
set_tests_properties(check_rift_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^rift: 100 steps from 0, against exact values\nrift\\.g\\[1,1\\]: at 1 the compiled step takes 'g_n\\[j<=2\\] \\| d_n\\[j\\] < c\\*j = 1' and the interpreter 'g_n\\[j<=2\\] = 0'\\; the guard of the first is exactly on its threshold\nrift\\.d\\[1,1\\]: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\nrift\\.g\\[1,1\\]: 1 at 1, where the interpreter gives 0\n$")
set_tests_properties(check_sill_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^sill: 100 steps from 0, against exact values\nsill\\.g: at 2 the compiled step takes 'g_m \\| d_\\(2\\*m\\) < c = 1' and the interpreter 'g_m = 0'\\; the guard of the first is exactly on its threshold\nsill\\.d: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\nsill\\.g: 1 at 2, where the interpreter gives 0\n$")
set_tests_properties(check_seam_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^seam: 100 steps from 0, against exact values\nseam\\.g\\[1,1\\]: at 2 the compiled step takes 'g_m\\[j<=2\\] \\| d_\\(2\\*m\\)\\[j\\] < c\\*j = 1' and the interpreter 'g_m\\[j<=2\\] = 0'\\; the guard of the first is exactly on its threshold\nseam\\.d\\[1,1\\]: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\nseam\\.g\\[1,1\\]: 1 at 2, where the interpreter gives 0\n$")
add_test(NAME check_calm COMMAND check_calm)
add_test(NAME check_wild COMMAND check_wild)
add_test(NAME check_brink COMMAND check_brink)
add_test(NAME check_rift COMMAND check_rift)
add_test(NAME check_sill COMMAND check_sill)
add_test(NAME check_seam COMMAND check_seam)
add_test(NAME check_pole COMMAND check_pole)
add_test(NAME check_huge COMMAND check_huge)
add_test(NAME check_masked COMMAND check_masked)
set_tests_properties(check_wild check_brink check_rift check_sill check_seam check_pole
                     check_huge check_masked PROPERTIES WILL_FAIL ON)

# A float target (DESIGN.md): pid and adc's headers as compile/expected/float
# has them, and compile/float.ink's checks, each built so that an operation done
# in double is a warning.
set(float_flags $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wdouble-promotion;-Wfloat-conversion>)
foreach(model IN ITEMS pid adc)
    set(header ${CMAKE_CURRENT_BINARY_DIR}/compile/float/${model}.h)
    add_custom_command(OUTPUT ${header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/compile/float
        COMMAND inkamath_cli --compile ${CMAKE_CURRENT_SOURCE_DIR}/compile/${model}.ink --float
                -o ${header}
        DEPENDS inkamath_cli compile/${model}.ink
        VERBATIM)
    add_custom_target(float_${model} ALL DEPENDS ${header})
    # Built alone only where a double operation is a warning.
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set_source_files_properties(${header} PROPERTIES LANGUAGE C
                                    COMPILE_OPTIONS -Wno-unused-function)
        add_library(compile_float_${model} OBJECT ${header})
        target_compile_options(compile_float_${model} PRIVATE ${float_flags})
        target_link_libraries(compile_float_${model} PRIVATE inkamath_warnings)
    endif()
    add_test(NAME compile_float_${model}_header
             COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol
                     ${CMAKE_CURRENT_SOURCE_DIR}/compile/expected/float/${model}.h ${header})
endforeach()

foreach(checked IN ITEMS drift:calm drift:wild drift:ledge logistic:gate momentum:ball newton:stiff
                         inputs:fan float:hum float:drone float:far float:tally float:crawl
                         float:steep float:blast
                         steady:mark softmax:cls)
    string(REPLACE ":" ";" checked ${checked})
    list(GET checked 0 file)
    list(GET checked 1 instance)
    set(program ${CMAKE_CURRENT_BINARY_DIR}/compile/float/${instance}.c)
    add_custom_command(OUTPUT ${program}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/compile/float
        COMMAND inkamath_cli --check ${CMAKE_CURRENT_SOURCE_DIR}/compile/${file}.ink ${instance}
                --float -o ${program}
        DEPENDS inkamath_cli compile/${file}.ink
        VERBATIM)
    add_executable(check_float_${instance} ${program})
    target_compile_options(check_float_${instance} PRIVATE ${float_flags})
    target_link_libraries(check_float_${instance} PRIVATE inkamath_warnings
                          $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)
    set_target_properties(check_float_${instance} PROPERTIES
                          C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    add_test(NAME check_float_${instance} COMMAND check_float_${instance})
    add_test(NAME check_float_${instance}_report COMMAND check_float_${instance})
endforeach()
set_tests_properties(check_float_wild check_float_ledge check_float_far check_float_crawl
                     check_float_steep PROPERTIES WILL_FAIL ON)
# Each report as compile/float.ink gives it; what a libm or the seeds decide,
# by its form.
set(in_float " in float, against exact values")
set(inexact " in float, against exact values until 0 and inexact ones from there")
set(units "[0-9.e+-]+, [0-9.]+ units of a float${about}")
set(parts "0\\.096026361 at 6, where the interpreter gives 0\\.10000000000000001\n")
foreach(report IN ITEMS
        "calm:^calm: 100 steps from 0${in_float}\ncalm\\.v: within 1\\.1e-06, 1\\.4 units of a float\n$"
        "wild:^wild: 100 steps from 0${in_float}\nwild\\.d: ${parts}$"
        "ledge:^ledge: 100 steps from 0${in_float}\nledge\\.g: at 1 the compiled step takes 'g_n \\| d_n < c - w = 1' and the interpreter 'g_n = 0'\\; the guard of the first is 1e-12 from its threshold\nledge\\.d: ${parts}ledge\\.g: 1 at 1, where the interpreter gives 0\n$"
        "gate:^gate: 100 steps from 0${inexact}\ngate\\.b: within 1\\.2e-06, 2\\.5 units of a float${about}\ngate\\.w: within 9\\.4e-07, 3\\.9 units of a float${about}\ngate\\.p: within 2\\.1e-07, 1\\.8 units of a float${about}\n$"
        "ball:^ball: 100 steps from 0${inexact}\n(ball\\.[a-z]+: within [0-9.e+-]+, [0-5](\\.[0-9]+)? units of a float${about}\n)+$"
        "stiff:^stiff: 100 steps from 0${inexact}\nstiff\\.h: within 6e-08, 0\\.5 units of a float\nstiff\\.x: within ${units}\nstiff\\.y: within ${units}\n$"
        "crawl:^crawl: 100 steps from 0${in_float}\ncrawl\\.s: within 0, 0 units of a float\ncrawl\\.y: 100000016 at 0, where the interpreter gives none: s did not converge within 100 terms \\(last term 100000800\\)\n$"
        "fan:^fan: 100 steps from 0${in_float}\nfan\\.y: within 0, 0 units of a float\n$"
        "hum:^hum: 100 steps from 0${in_float}\nhum\\.y: within 2\\.7e-06, 22 units of a float\n$"
        "drone:^drone: 100 steps from 0${in_float}\ndrone\\.y: within 5\\.6e-06, 47 units of a float\n$"
        "far:^far: 100 steps from 0${in_float}\nfar\\.x: within 0\\.99, 16 units of a float\nfar\\.d: 0 at 1, where the interpreter gives 0\\.01\n$"
        "tally:^tally: 100 steps from 0${in_float}\ntally\\.c: within 50, 25 units of a float\n$"
        "steep:^steep: 100 steps from 0${inexact}\nsteep\\.e: inf at 67, where the interpreter gives [0-9.e+-]+\; the interpreter's term about [0-9.e+-]+ from the exact one\n$"
        "blast:^blast: 100 steps from 0${in_float}\nblast\\.y: within 0, 0 units of a float\nblast\\.z: within 0, 0 units of a float\n$"
        "mark:^mark: 100 steps from 0${inexact}\nmark\\.d: within ${units}\nmark\\.s: within ${units}\n$"
        "cls:^cls: 100 steps from 0${inexact}\ncls\\.z: within 2\\.4e-08, 0\\.2 units of a float\ncls\\.q: within 1\\.9e-06, 1\\.5 units of a float${about}\ncls\\.s: within 6\\.3e-08, 0\\.53 units of a float${about}\ncls\\.loss: within 4\\.1e-07, 3\\.4 units of a float${about}\n$")
    string(FIND "${report}" ":" colon)
    string(SUBSTRING "${report}" 0 ${colon} instance)
    math(EXPR colon "${colon} + 1")
    string(SUBSTRING "${report}" ${colon} -1 expected)
    set_tests_properties(check_float_${instance}_report PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
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
