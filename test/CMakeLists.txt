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

# One ctest entry per doctest suite, and one per golden transcript, each
# its test case of the same name: a crash reports as a single failure rather
# than taking the others' results with it, the goldens run in parallel, and
# each timeout is a tripwire for one file rather than for the sum of them.
# The goldens are the files interpreter_test.cpp replays; the others in
# data/ are libraries they load.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS interpreter_test.cpp)
file(STRINGS interpreter_test.cpp goldens REGEX "check_transcript\\(\"[a-z_]+\\.ink\"\\)")
set(golden_cases "")
foreach(golden IN LISTS goldens)
    string(REGEX REPLACE ".*\"([a-z_]+)\\.ink\".*" "\\1" case "${golden}")
    list(APPEND golden_cases ${case})
    add_test(NAME ${case}.ink COMMAND inkamath_tests --test-suite=interpreter
             --test-case=${case} --no-intro)
    # A golden without its case runs none, which is a failure, not a pass.
    set_tests_properties(${case}.ink PROPERTIES TIMEOUT 60
                         FAIL_REGULAR_EXPRESSION "test cases: +0 \\|")
endforeach()
# fastgrad.ink's 600,000 remembered calls take 25 s under sanitizers alone.
set_tests_properties(fastgrad.ink PROPERTIES TIMEOUT 120)
foreach(suite IN ITEMS bignum line_editor matrix)
    add_test(NAME ${suite} COMMAND inkamath_tests --test-suite=${suite} --no-intro)
    set_tests_properties(${suite} PROPERTIES TIMEOUT 60)
endforeach()
# The interpreter's cases that are not goldens: README.md's sessions and the rest.
string(REPLACE ";" "," golden_cases "${golden_cases}")
add_test(NAME interpreter COMMAND inkamath_tests --test-suite=interpreter --no-intro
         --test-case-exclude=${golden_cases})
set_tests_properties(interpreter PROPERTIES TIMEOUT 60)

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
                         drift:rift drift:naught drift:zeroed drift:mirrored drift:nests drift:latch drift:toil
                         drift:late drift:radix
                         delay:fir newton:stiff logistic:gate sized:sized softmax:cls steady:mark
                         rnn:rnn attention:head momentum:ball adam:fit lqr:gain
                         rates:pairs rates:odd seeded:warm cascade:servo decimate:boxcar
                         decimate:stride decimate:frame decimate:lead
                         decimate:trail drift:sill drift:seam history:past history:twin
                         history:lit history:rise history:ajar history:sown history:nest history:kin history:vast
                         history:twice history:relayed history:pole
                         history:huge history:sever history:veiled history:cloaked history:two history:hid history:masked
                         nan:run nan:sign nan:shy nan:slope nan:pair
                         nan:lap nan:trim nan:any nan:cell nan:held nan:idle nan:imag inputs:fan inputs:grid
                         inputs:chan inputs:twirl inputs:duo inputs:mean3 inputs:mean2
                         inputs:track inputs:twain inputs:nestv inputs:fed estimate:tie
                         estimate:chaos estimate:dyad estimate:flat estimate:halved
                         estimate:steep estimate:giant elementary:els trig:tri
                         grad:line grad:fall grad:hinge grad:edge grad:clip grad:steer grad:saw
                         grad:ring grad:notch grad:apart grad:smooth grad:cusp grad:over
                         tensor:heads tensor:whirl tensor:batch tensor:spread
                         tensor:speck cellguards:rfit cellguards:tfit cellguards:layer
                         cellguards:kink cellguards:crest cellguards:mask cellguards:order
                         grad:swing thresholds:dies thresholds:beat stable:moments stable:peak
                         reddi:thm1 reddi:fixed reddi:gen
                         kahan:sums kahan:muller kahan:rounded kahan:surprise kahan:solveur
                         rump:rump signatures:inv
                         goldberg:year goldberg:needled goldberg:big goldberg:seven
                         iterates:poly iterates:thirds iterates:plan iterates:graph iterates:gaps
                         iterates:memo iterates:unread iterates:lone conv:see conv:learn)
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
# header's own, compiled once (DESIGN.md): every term the interpreter's. A
# power of 2 is tested for NaN by its exponent, not by a second pow.
set_tests_properties(check_els_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^els: 100 steps from 0, against exact values until 0 and inexact ones from there\nels\\.e: within 0${about}\nels\\.g: within 0${about}\nels\\.h: within 0${about}\nels\\.l: within 0${about}\nels\\.t: within 0${about}\n$")
add_test(NAME check_els_functions
         COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/compile/els.c)
set_tests_properties(check_els_functions PROPERTIES
    PASS_REGULAR_EXPRESSION "els_exp\\(double arg_x\\).*els_log\\(double arg_x\\).*els_tanh\\(double arg_x\\)"
    FAIL_REGULAR_EXPRESSION "pow\\(2\\.718|isnan\\(pow\\(")
# sin, cos, abs, max and min as the prelude writes them (DESIGN.md), and
# grad's parts through each: every term the interpreter's.
set_tests_properties(check_tri_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^tri: 100 steps from 0, against exact values until 0 and inexact ones from there\ntri\\.c: within 0${about}\ntri\\.g: within 0${about}\ntri\\.q: within 0${about}\ntri\\.s: within 0${about}\ntri\\.b: within 0${about}\ntri\\.l: within 0${about}\ntri\\.m: within 0${about}\ntri\\.v: within 0${about}\ntri\\.w: within 0${about}\n$")
add_test(NAME check_tri_functions
         COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/compile/tri.c)
set_tests_properties(check_tri_functions PROPERTIES
    PASS_REGULAR_EXPRESSION "tri_abs\\(double arg_x\\).*tri_sin\\(double arg_x\\).*tri_cos\\(double arg_x\\).*tri_max\\(double arg_a, double arg_b\\).*tri_max_da\\(.*tri_abs_dx\\(.*tri_cos_dx\\(.*tri_min\\(double arg_a, double arg_b\\).*tri_min_db\\(.*tri_sin_dx\\("
    FAIL_REGULAR_EXPRESSION "[^_a-z](sin|cos)\\(")
# grad compiled (DESIGN.md), each value carrying its part: every gradient
# the interpreter's, as compile/grad.ink gives them.
set_tests_properties(check_line_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^line: 100 steps from 0, against exact values\nline\\.w: within 4\\.4e-16\n$")
set_tests_properties(check_fall_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^fall: 100 steps from 0, against exact values until 1 and inexact ones from there\nfall\\.u: within 0${about}\nfall\\.w: within 0${about}\n$")
set_tests_properties(check_hinge_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^hinge: 100 steps from 0, against exact values\nhinge\\.g: within 0\n$")
set_tests_properties(check_edge_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^edge: 100 steps from 0, against exact values until 0 and inexact ones from there\nedge\\.c: within 0\nedge\\.d: within 0\nedge\\.e: within 0${about}\nedge\\.f: within 0\nedge\\.g: within 0\nedge\\.h: within 0${about}\nedge\\.i: within 0\nedge\\.j: no value compared, the step NaN where the interpreter gives none, as at 0: a comparison jumps at t = 1\nedge\\.k: within 0\nedge\\.l: within 0\nedge\\.m: within 0\nedge\\.o: no value compared, the step NaN where the interpreter gives none, as at 0: floor jumps at t = 2\nedge\\.s: within 0${about}\nedge\\.v: no value compared, the step NaN where the interpreter gives none, as at 0: a power's derivative is infinite at t = 0\nedge\\.w: within 0\n$")
set_tests_properties(check_clip_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^clip: 100 steps from 0, against exact values\nclip\\.w: within 0\n$")
set_tests_properties(check_steer_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^steer: 100 steps from 0, against exact values until 1 and inexact ones from there\nsteer\\.x: within 0${about}\n$")
set_tests_properties(check_saw_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^saw: 100 steps from 0, against exact values\nsaw\\.b: within 0\nsaw\\.h: within 0\n$")
set_tests_properties(check_ring_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^ring: 100 steps from 0, against exact values\nring\\.a: within 0\nring\\.b: within 0\nring\\.c: within 0\n$")
set_tests_properties(check_notch_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^notch: 100 steps from 0, against exact values\nnotch\\.y: within 0\n$")
set_tests_properties(check_smooth_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^smooth: 100 steps from 0, against exact values\nsmooth\\.z: no value compared, the step NaN where the interpreter gives none, as at 0: floor jumps at t = ~0\\.34657359\n$")
set_tests_properties(check_cusp_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^cusp: 100 steps from 0, against exact values until 0 and inexact ones from there\ncusp\\.x: within 0\ncusp\\.g: within 0\n$")
set_tests_properties(check_swing_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^swing: 100 steps from 0, against exact values until 1 and inexact ones from there\n(swing\\.[A-Za-z]+: within [0-9.e+-]+(, from 1)?${about}\n)+$")
set_tests_properties(check_over_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^over: 100 steps from 0, against exact values until 0 and inexact ones from there\nover\\.y: within 1\\.4e-20\nover\\.z: within [0-9.e+-]+${about}\n$")
# Its known limit, C110, held where it parts.
set_tests_properties(check_apart_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^apart: 100 steps from 0, against exact values\napart\\.y: [-a-z()]+ at 0, where the interpreter gives 0\n$")
add_test(NAME check_apart COMMAND check_apart)
set_tests_properties(check_apart PROPERTIES WILL_FAIL ON)
# Tensors compiled (DESIGN.md), as compile/tensor.ink gives them, and a cell
# that parts named by its slice, row and column.
set_tests_properties(check_heads_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^heads: 100 steps from 0, against exact values until 0 and inexact ones from there\nheads\\.O: within (0|[1-9](\\.[0-9])?e-1[4-9])${about}\n$")
set_tests_properties(check_whirl_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^whirl: 100 steps from 0, against exact values\nwhirl\\.d: within 0\nwhirl\\.s: within 0\nwhirl\\.c: within 0\nwhirl\\.e: within 0, from 1\nwhirl\\.g: within 0\nwhirl\\.h: within 0, from 1\nwhirl\\.k: within 0\nwhirl\\.l: within 0\nwhirl\\.p: within 0\nwhirl\\.q: within 0\nwhirl\\.u: within 0, from 1\n$")
set_tests_properties(check_batch_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^batch: 100 steps from 0, against exact values\nbatch\\.w: within 0\n$")
set_tests_properties(check_speck_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^speck: 100 steps from 0, against exact values\nspeck\\.y: within 0\n$")
set_tests_properties(check_spread_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^spread: 100 steps from 0, against exact values\nspread\\.d\\[2,1,2\\]: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\n$")
add_test(NAME check_spread COMMAND check_spread)
set_tests_properties(check_spread PROPERTIES WILL_FAIL ON)
# Guards on cells at run time (DESIGN.md), as compile/cellguards.ink gives
# them, and crest parting by its values alone.
set_tests_properties(check_rfit_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^rfit: 100 steps from 0, against exact values\nrfit\\.W: within 0\nrfit\\.L: within 0\n$")
set_tests_properties(check_tfit_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^tfit: 100 steps from 0, against exact values\ntfit\\.W: within 0\ntfit\\.H: within 0\ntfit\\.L: within 0\n$")
set_tests_properties(check_layer_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^layer: 100 steps from 0, against exact values\nlayer\\.w: within 0\nlayer\\.z: within 0\nlayer\\.h: within 0\nlayer\\.y: within 0\n$")
set_tests_properties(check_kink_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^kink: 100 steps from 0, against exact values\nkink\\.c: within 0\nkink\\.d: within 0\nkink\\.e: within 0\nkink\\.g: within 0\nkink\\.h: within 0\nkink\\.m: within 0\n$")
set_tests_properties(check_toil_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^toil: 100 steps from 0, against exact values until 0 and inexact ones from there\ntoil\\.y: within 0\; the interpreter ran out of steps at 2, not compared\n$")
set_tests_properties(check_latch_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^latch: 100 steps from 0, against exact values\nlatch\\.m: within 0, from 1\nlatch\\.w: within 0\n$")
set_tests_properties(check_late_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^late: 100 steps from 0, against exact values\nlate\\.v: within 0\nlate\\.x: within 0, from 1\n$")
# Reddi, Kale and Kumar's iterates from 1, their steps from 2.
set(until2 ", against exact values until 2 and inexact ones from there")
set_tests_properties(check_thm1_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^thm1: 100 steps from 0${until2}\nthm1\\.h: within 0, from 2${about}\nthm1\\.x: within 0, from 1${about}\nthm1\\.R: within 0${about}\nthm1\\.g: within 0, from 1\nthm1\\.m: within 0\nthm1\\.v: within 2\\.2e-16\n$")
set_tests_properties(check_fixed_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^fixed: 100 steps from 0${until2}\nfixed\\.x: within 0, from 1${about}\nfixed\\.R: within 0${about}\nfixed\\.g: within 0, from 1\nfixed\\.v: within 2\\.2e-16\nfixed\\.w: within 0\n$")
set_tests_properties(check_gen_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^gen: 100 steps from 0${until2}\ngen\\.h: within 0, from 2${about}\ngen\\.x: within 0, from 1${about}\ngen\\.R: within 0${about}\ngen\\.g: within 0, from 1\ngen\\.m: within 1\\.1e-16\ngen\\.v: within 0\n$")
# Kahan (2006): Muller's recurrence parting where the paper's table has it,
# and Rump's example (2010) at once; compile/kahan.ink and rump.ink.
set(inexact0 ", against exact values until 0 and inexact ones from there")
set_tests_properties(check_sums_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^sums: 100 steps from 0, against exact values\nsums\\.c: within 9e-13, from 1\nsums\\.o: within 3\\.6e-12\nsums\\.plain: within 5\\.5e-12\nsums\\.s: within 1\\.8e-12\nsums\\.comp: within 9e-13\nsums\\.total: within 1\\.8e-12\n$")
set_tests_properties(check_muller_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^muller: 100 steps from 0, against exact values\nmuller\\.x: 4\\.9455373955305078 at 7, where the interpreter gives 4\\.9455374041239164\n$")
set_tests_properties(check_rounded_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^rounded: 100 steps from 0${inexact0}\nrounded\\.x: within 0${about}, past the tolerance from 7\n$")
set_tests_properties(check_surprise_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^surprise: 100 steps from 0${inexact0}\nsurprise\\.g: within 0${about}, past the tolerance from 0\nsurprise\\.k: within 0${about}\n$")
set_tests_properties(check_solveur_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^solveur: 100 steps from 0, against exact values\nsolveur\\.d: within 0\nsolveur\\.z: within 0\n$")
set_tests_properties(check_rump_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^rump: 100 steps from 0, against exact values\nrump\\.g: 1\\.1726039400531787 at 0, where the interpreter gives -0\\.82739605994682142\n$")
# Goldberg (1991): Heron's formula parting at a needle, and (x + y) + z
# from x + (y + z) at 1e30, as the paper has them; compile/goldberg.ink.
set_tests_properties(check_year_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^year: 100 steps from 0${inexact0}\nyear\\.naive: within 0${about}\nyear\\.thm4: within 0${about}\n$")
set_tests_properties(check_needled_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^needled: 100 steps from 0${inexact0}\nneedled\\.heron: 8\\.0466270446777376e-07 at 0, where the interpreter gives 6\\.9685834354698204e-07\\; the interpreter's term about [0-9.e+-]+ from the exact one\nneedled\\.kahan: within 0${about}\n$")
set_tests_properties(check_big_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^big: 100 steps from 0, against exact values\nbig\\.s: within 2e-30\nbig\\.t: within 0\nbig\\.w: 0 at 0, where the interpreter gives 1\n$")
set_tests_properties(check_seven_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^seven: 100 steps from 0, against exact values\nseven\\.t: within 0\nseven\\.u: within 1\\.4e-14\n$")
set_tests_properties(check_mask_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^mask: 100 steps from 0, against exact values\nmask\\.y: within 0\n$")
set_tests_properties(check_order_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^order: 100 steps from 0, against exact values\norder\\.k: within 0\norder\\.p: within 0\norder\\.s: within 0\n$")
set_tests_properties(check_crest_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^crest: 100 steps from 0, against exact values\ncrest\\.d\\[1,1\\]: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\ncrest\\.g\\[1,1\\]: 1 at 1, where the interpreter gives 0\n$")
add_test(NAME check_crest COMMAND check_crest)
set_tests_properties(check_crest PROPERTIES WILL_FAIL ON)
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
set_tests_properties(check_inv_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^inv: 100 steps from 0, against exact values\ninv\\.A: within 0\ninv\\.c: within 0\ninv\\.d: within 0\ninv\\.g: within 0\ninv\\.t: within 0\ninv\\.u: within 0\ninv\\.v: within 0\ninv\\.o: within 0\ninv\\.s: within 0\n$")
# Each size its signature names folds where tr is called, so the header is the
# one written with every size a number, byte for byte (DESIGN.md, a size bound
# by a signature); tr2 is tr at 2x2.
file(READ compile/signatures.ink signatures_named)
set(signatures_numbered "${signatures_named}")
function(numbered named number)
    string(FIND "${signatures_numbered}" "${named}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "compile/signatures.ink no longer has ${named}")
    endif()
    string(REPLACE "${named}" "${number}" signatures_numbered "${signatures_numbered}")
    set(signatures_numbered "${signatures_numbered}" PARENT_SCOPE)
endfunction()
numbered("tr(M[j<=n, k<=n]) = sum_(j=1)^n" "tr(M) = sum_(j=1)^3")
numbered("c2(M[j<=n, k<=n])" "tr2(M) = sum_(j=1)^2 M[j,j]\n    c2(M)")
numbered("sq(v[j<=n]) = sum_(j=1)^n" "sq(v) = sum_(j=1)^3")
numbered("outer(x[j<=m], y[k<=n])[j<=m, k<=n]" "outer(x, y)[j<=3, k<=2]")
numbered("u_n = tr([n" "u_n = tr2([n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS compile/signatures.ink)
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/compile/numbered/signatures.ink
     CONTENT "${signatures_numbered}" @ONLY)
foreach(variant IN ITEMS named numbered)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/compile/signatures.ink)
    if(variant STREQUAL numbered)
        set(source ${CMAKE_CURRENT_BINARY_DIR}/compile/numbered/signatures.ink)
    endif()
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/compile/${variant}/signatures.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/compile/${variant}
        COMMAND inkamath_cli --compile ${source} invariants
                -o ${CMAKE_CURRENT_BINARY_DIR}/compile/${variant}/signatures.h
        DEPENDS inkamath_cli ${source}
        VERBATIM)
endforeach()
add_custom_target(signatures_headers ALL
                  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/compile/named/signatures.h
                          ${CMAKE_CURRENT_BINARY_DIR}/compile/numbered/signatures.h)
add_test(NAME compile_signatures_header
         COMMAND ${CMAKE_COMMAND} -E compare_files
                 ${CMAKE_CURRENT_BINARY_DIR}/compile/named/signatures.h
                 ${CMAKE_CURRENT_BINARY_DIR}/compile/numbered/signatures.h)
# A sequence with parameters read at a constant index (DESIGN.md), as
# compile/iterates.ink gives it; graph's estimate by its form, inf where a
# disturbed run rounds a root above 1.
set_tests_properties(check_poly_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^poly: 100 steps from 0, against exact values\npoly\\.A: within 0\npoly\\.c: within 0\npoly\\.d: within 0\npoly\\.g: within 0\n$")
set_tests_properties(check_thirds_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^thirds: 100 steps from 0${inexact0}\nthirds\\.A: within 0${about}\nthirds\\.c: within 0${about}\nthirds\\.d: within 0${about}\nthirds\\.g: within 0${about}\n$")
set_tests_properties(check_plan_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^plan: 100 steps from 0, against exact values\nplan\\.y: within 0\n$")
set_tests_properties(check_graph_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^graph: 100 steps from 0${inexact0}\ngraph\\.h: within 0\\; the interpreter's terms about ([0-9.e+-]+|inf) from the exact ones, past the tolerance from [0-9]+\n$")
set_tests_properties(check_gaps_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^gaps: 100 steps from 0, against exact values\ngaps\\.a: within 0\ngaps\\.b: within 0\n$")
set_tests_properties(check_memo_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^memo: 100 steps from 0, against exact values\nmemo\\.a: within 0\nmemo\\.b: within 0\nmemo\\.d: within 0\nmemo\\.e: within 0\nmemo\\.l: within 0\n$")
# A convolutional network's forward pass and its training (Goodfellow et al.
# 2016, §9.5), as compile/conv.ink gives them.
set_tests_properties(check_see_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^see: 100 steps from 0, against exact values\nsee\\.Xp: within 0\nsee\\.Z: within 0\nsee\\.M: within 0\nsee\\.s: within 0\n$")
set_tests_properties(check_learn_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^learn: 100 steps from 0, against exact values\nlearn\\.K: within 0\nlearn\\.W: within 0\nlearn\\.a: within 0\nlearn\\.d: within 0\nlearn\\.E: within [1-9](\\.[0-9])?e-1[4-6]\n$")
set_tests_properties(check_unread_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^unread: 100 steps from 0, against exact values\nunread\\.y: within 0\n$")
set_tests_properties(check_lone_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^lone: 100 steps from 0, against exact values\nlone\\.y: within 0\nlone\\.z: within 0\n$")
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
    "^trim: 100 steps from 0, against exact values\ntrim\\.y: no value compared, the step NaN where the interpreter gives none, as at 0: no clause of r applies\n$")
set_tests_properties(check_any_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^any: 100 steps from 0, against exact values\nany\\.y: within 0\nany\\.o: within 0\n$")
set_tests_properties(check_cell_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^cell: 100 steps from 0, against exact values\ncell\\.b: within 0\ncell\\.c: within 0\ncell\\.m: within 0\ncell\\.p: within 0\n$")
set_tests_properties(check_held_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^held: 100 steps from 0, against exact values\nheld\\.d: no value compared, the step NaN where the interpreter gives none, as at 0: no clause of r applies\n$")
set_tests_properties(check_idle_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^idle: 100 steps from 0, against exact values\nidle\\.g: within 0\nidle\\.h: within 0\nidle\\.k: within 0\nidle\\.l: within 0\nidle\\.y: within 0\n$")
set_tests_properties(check_imag_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^imag: 100 steps from 0, against [^\n]*\nimag\\.x: within 0\nimag\\.z: within 0\n$")
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
set_tests_properties(check_hid_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^hid: 100 steps from 0, against exact values\nhid\\.c_capped\\.u: within 0\nhid\\.c_capped\\.c: within 0\nhid\\.c: within 0\n$")
set_tests_properties(check_masked_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^masked: 100 steps from 0, against exact values\nmasked\\.e_tenth\\.d: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\nmasked\\.e: [0-9.e+-]+ at 9, where the interpreter gives 0\\.10000000000000001\n$")
set_tests_properties(check_pole_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^pole: 100 steps from 0, against exact values\npole\\.y: inf at 3, where the interpreter gives none: division by zero\n$")
set_tests_properties(check_huge_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^huge: 100 steps from 0, against exact values\nhuge\\.y: inf at 31, where the interpreter's term is too large for a double\n$")
set_tests_properties(check_sever_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^sever: 100 steps from 0, against exact values until 10 and inexact ones from there\nsever\\.p\\[2,2\\]: inf at 12, where the interpreter's term is not a finite number\n$")
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
set_tests_properties(check_zeroed_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^zeroed: 100 steps from 0, against exact values\nzeroed\\.y: within 0\n$")
set_tests_properties(check_mirrored_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^mirrored: 100 steps from 0, against exact values\nmirrored\\.y: within 0\n$")
set_tests_properties(check_radix_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^radix: 100 steps from 0, against exact values\nradix\\.y: within 0\n$")
set_tests_properties(check_nests_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^nests: 100 steps from 0, against exact values\nnests\\.s: within 0\nnests\\.x: within 0\n$")
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
# compile/thresholds.ink and stable.ink in double: each threshold met as the
# interpreter meets it, and the textbook variance parting at once.
set_tests_properties(check_dies_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^dies: 100 steps from 0, against exact values\ndies\\.b: within 0\ndies\\.alive: within 0\n$")
set_tests_properties(check_beat_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^beat: 100 steps from 0, against exact values\n(beat\\.(x|xh|e): within [0-9.e+-]+\n)+beat\\.locked: within 0\n$")
set_tests_properties(check_moments_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^moments: 100 steps from 0, against exact values\n(moments\\.(S1|S2|mu|M2): within [0-9.e+-]+\n)+moments\\.naive: [0-9.e+-]+ at 1, where the interpreter gives 0\\.1225\nmoments\\.welford: within [0-9.e+-]+\n$")
set_tests_properties(check_peak_report PROPERTIES PASS_REGULAR_EXPRESSION
    "^peak: 100 steps from 0, against exact values until 0 and inexact ones from there\npeak\\.Q: within 0\n(peak\\.[SAB]: within [0-9.e+-]+${about}\n)+$")
add_test(NAME check_moments COMMAND check_moments)
add_test(NAME check_calm COMMAND check_calm)
add_test(NAME check_wild COMMAND check_wild)
add_test(NAME check_brink COMMAND check_brink)
add_test(NAME check_rift COMMAND check_rift)
add_test(NAME check_sill COMMAND check_sill)
add_test(NAME check_seam COMMAND check_seam)
add_test(NAME check_pole COMMAND check_pole)
add_test(NAME check_huge COMMAND check_huge)
add_test(NAME check_masked COMMAND check_masked)
add_test(NAME check_sever COMMAND check_sever)
add_test(NAME check_muller COMMAND check_muller)
add_test(NAME check_rump COMMAND check_rump)
add_test(NAME check_needled COMMAND check_needled)
add_test(NAME check_big COMMAND check_big)
set_tests_properties(check_wild check_brink check_rift check_sill check_seam check_pole
                     check_huge check_masked check_sever check_moments check_muller check_rump
                     check_needled check_big PROPERTIES WILL_FAIL ON)

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
                         drift:radix inputs:fan float:hum float:drone float:far float:tally float:crawl
                         float:steep float:blast
                         steady:mark softmax:cls grad:line tensor:whirl tensor:batch
                         cellguards:rfit cellguards:tfit thresholds:dies thresholds:beat stable:peak
                         reddi:thm1 reddi:fixed reddi:gen
                         kahan:sums kahan:muller kahan:rounded kahan:surprise kahan:solveur rump:rump
                         goldberg:year goldberg:needle goldberg:big goldberg:seven iterates:poly
                         conv:see)
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
                     check_float_steep check_float_dies check_float_beat check_float_peak
                     check_float_muller check_float_rounded check_float_surprise
                     check_float_solveur check_float_rump check_float_needle check_float_big
                     PROPERTIES WILL_FAIL ON)
# Each report as compile/float.ink gives it; what a libm or the seeds decide,
# by its form.
set(in_float " in float, against exact values")
set(inexact " in float, against exact values until 0 and inexact ones from there")
set(units "[0-9.e+-]+, [0-9.]+ units of a float${about}")
set(parts "0\\.096026361 at 6, where the interpreter gives 0\\.10000000000000001\n")
set(exact "within 0, 0 units of a float")
foreach(report IN ITEMS
        "calm:^calm: 100 steps from 0${in_float}\ncalm\\.v: within 1\\.1e-06, 1\\.4 units of a float\n$"
        "wild:^wild: 100 steps from 0${in_float}\nwild\\.d: ${parts}$"
        "ledge:^ledge: 100 steps from 0${in_float}\nledge\\.g: at 1 the compiled step takes 'g_n \\| d_n < c - w = 1' and the interpreter 'g_n = 0'\\; the guard of the first is 1e-12 from its threshold\nledge\\.d: ${parts}ledge\\.g: 1 at 1, where the interpreter gives 0\n$"
        "gate:^gate: 100 steps from 0${inexact}\ngate\\.b: within 1\\.2e-06, 2\\.5 units of a float${about}\ngate\\.w: within 9\\.4e-07, 3\\.9 units of a float${about}\ngate\\.p: within 2\\.1e-07, 1\\.8 units of a float${about}\n$"
        "ball:^ball: 100 steps from 0${inexact}\n(ball\\.[a-z]+: within [0-9.e+-]+, [0-5](\\.[0-9]+)? units of a float${about}\n)+$"
        "stiff:^stiff: 100 steps from 0${inexact}\nstiff\\.h: within 6e-08, 0\\.5 units of a float\nstiff\\.x: within ${units}\nstiff\\.y: within ${units}\n$"
        "crawl:^crawl: 100 steps from 0${in_float}\ncrawl\\.s: within 0, 0 units of a float\ncrawl\\.y: 100000016 at 0, where the interpreter gives none: s did not converge within 100 terms \\(last term 100000800\\)\n$"
        "fan:^fan: 100 steps from 0${in_float}\nfan\\.y: within 0, 0 units of a float\n$"
        "radix:^radix: 100 steps from 0${in_float}\nradix\\.y: ${exact}\n$"
        "hum:^hum: 100 steps from 0${in_float}\nhum\\.y: within 2\\.7e-06, 22 units of a float\n$"
        "drone:^drone: 100 steps from 0${in_float}\ndrone\\.y: within 5\\.6e-06, 47 units of a float\n$"
        "far:^far: 100 steps from 0${in_float}\nfar\\.x: within 0\\.99, 16 units of a float\nfar\\.d: 0 at 1, where the interpreter gives 0\\.01\n$"
        "tally:^tally: 100 steps from 0${in_float}\ntally\\.c: within 50, 25 units of a float\n$"
        "steep:^steep: 100 steps from 0${inexact}\nsteep\\.e: inf at 67, where the interpreter gives [0-9.e+-]+\; the interpreter's term about [0-9.e+-]+ from the exact one\n$"
        "blast:^blast: 100 steps from 0${in_float}\nblast\\.y: no value compared, the step NaN where the interpreter gives none, as at 0: p did not converge within 100 terms \\(last term [^)]+\\)\nblast\\.z: no value compared, the step NaN where the interpreter gives none, as at 0: v did not converge within 100 terms \\(last term [^)]+\\)\n$"
        "mark:^mark: 100 steps from 0${inexact}\nmark\\.d: within ${units}\nmark\\.s: within ${units}\n$"
        "line:^line: 100 steps from 0${in_float}\nline\\.w: within 2\\.7e-07, 1\\.9 units of a float\n$"
        "whirl:^whirl: 100 steps from 0${in_float}\nwhirl\\.d: ${exact}\nwhirl\\.s: ${exact}\nwhirl\\.c: ${exact}\nwhirl\\.e: ${exact}, from 1\nwhirl\\.g: ${exact}\nwhirl\\.h: ${exact}, from 1\nwhirl\\.k: ${exact}\nwhirl\\.l: ${exact}\nwhirl\\.p: ${exact}\nwhirl\\.q: ${exact}\nwhirl\\.u: ${exact}, from 1\n$"
        "batch:^batch: 100 steps from 0${in_float}\nbatch\\.w: ${exact}\n$"
        "rfit:^rfit: 100 steps from 0${in_float}\nrfit\\.W: ${exact}\nrfit\\.L: ${exact}\n$"
        "tfit:^tfit: 100 steps from 0${in_float}\ntfit\\.W: ${exact}\ntfit\\.H: ${exact}\ntfit\\.L: ${exact}\n$"
        "dies:^dies: 100 steps from 0${in_float}\ndies\\.alive: at 3 the compiled step takes 'alive_n \\| b_n > 0 = 1' and the interpreter 'alive_n = 0'\\; the guard of the first is exactly on its threshold\ndies\\.b: -0\\.0728999898 at 4, where the interpreter gives 0\ndies\\.alive: 1 at 3, where the interpreter gives 0\n$"
        "beat:^beat: 100 steps from 0${in_float}\nbeat\\.locked: at 2 the compiled step takes 'locked_n = 0' and the interpreter 'locked_n \\| e_n\\[1\\] == 0 and e_n\\[2\\] == 0 = 1'\\; the guard of the second is exactly on its threshold\n(beat\\.(x|xh|e): within [0-9.e+-]+, [0-9.e+-]+ units of a float\n)+beat\\.locked: 0 at 2, where the interpreter gives 1\n$"
        "peak:^peak: 100 steps from 0${inexact}\npeak\\.Q: ${exact}\npeak\\.S: within ${units}\npeak\\.A\\[3,1\\]: [-a-z()]+ at 63, where the interpreter gives 1\\; the interpreter's term about [0-9.e+-]+ from the exact one\npeak\\.B: within ${units}\n$"
        "thm1:^thm1: 100 steps from 0 in float${until2}\nthm1\\.h: within 1\\.2e-07, 1 units of a float, from 2${about}\nthm1\\.x: within 6\\.8e-08, 0\\.57 units of a float, from 1${about}\nthm1\\.R: within 9\\.3e-06, 2\\.4 units of a float${about}\nthm1\\.g: ${exact}, from 1\nthm1\\.m: ${exact}\nthm1\\.v: within 1e-06, 1\\.7 units of a float\n$"
        "fixed:^fixed: 100 steps from 0 in float${until2}\nfixed\\.x: within 1\\.3e-07, 1\\.1 units of a float, from 1${about}\nfixed\\.R: within 4\\.3e-06, 2\\.2 units of a float${about}\nfixed\\.g: ${exact}, from 1\nfixed\\.v: within 1e-06, 1\\.7 units of a float\nfixed\\.w: within 1e-06, 1\\.1 units of a float\n$"
        "gen:^gen: 100 steps from 0 in float${until2}\ngen\\.h: within 1\\.8e-07, 1\\.5 units of a float, from 2${about}\ngen\\.x: within 1\\.6e-07, 1\\.4 units of a float, from 1${about}\ngen\\.R: within 2e-06, 15 units of a float${about}\ngen\\.g: ${exact}, from 1\ngen\\.m: within 4\\.8e-07, 1 units of a float\ngen\\.v: within 7\\.2e-06, 0\\.47 units of a float\n$"
        "sums:^sums: 100 steps from 0${in_float}\nsums\\.c: within 0\\.00048, 4\\.1e\\+03 units of a float, from 1\nsums\\.o: within 0\\.0029, 2\\.9 units of a float\nsums\\.plain: within 0\\.0028, 2\\.8 units of a float\nsums\\.s: within 0\\.00047, 0\\.48 units of a float\nsums\\.comp: within 0\\.00048, 4\\.1e\\+03 units of a float\nsums\\.total: within 0\\.00051, 0\\.52 units of a float\n$"
        "muller:^muller: 100 steps from 0${in_float}\nmuller\\.x: 4\\.98312378 at 6, where the interpreter gives 4\\.9108474990827933\n$"
        "rounded:^rounded: 100 steps from 0${inexact}\nrounded\\.x: 4\\.98312378 at 6, where the interpreter gives 4\\.9108474986606296\; the interpreter's term about [0-9.e+-]+ from the exact one\n$"
        "surprise:^surprise: 100 steps from 0${inexact}\nsurprise\\.g: 1 at 0, where the interpreter gives 0\; the interpreter's term about [0-9.e+-]+ from the exact one\nsurprise\\.k: ${exact}${about}\n$"
        "solveur:^solveur: 100 steps from 0${in_float}\nsolveur\\.d: 0 at 0, where the interpreter gives -1\nsolveur\\.z\\[1,1\\]: [-a-z()]+ at 0, where the interpreter gives 12582909\n$"
        "rump:^rump: 100 steps from 0${in_float}\nrump\\.g: 1\\.17260396 at 0, where the interpreter gives -0\\.82739605994682142\n$"
        "year:^year: 100 steps from 0${inexact}\nyear\\.naive: within [0-9.]+, [1-9](\\.[0-9])?e\\+02 units of a float${about}\nyear\\.thm4: within [0-9.e-]+, [1-9](\\.[0-9])? units of a float${about}\n$"
        "needle:^needle: 100 steps from 0${inexact}\nneedle\\.heron: 0\\.0186444204 at 0, where the interpreter gives 0\\.016146540115034271\; the interpreter's term about [0-9.e+-]+ from the exact one\nneedle\\.kahan: within 5\\.3e-10, 0\\.0044 units of a float${about}\n$"
        "big:^big: 100 steps from 0${in_float}\nbig\\.s: within 2e-30, 1\\.7e-23 units of a float\nbig\\.t: ${exact}\nbig\\.w: 0 at 0, where the interpreter gives 1\n$"
        "seven:^seven: 100 steps from 0${in_float}\nseven\\.t: ${exact}\nseven\\.u: within 3\\.8e-06, 32 units of a float\n$"
        "poly:^poly: 100 steps from 0${in_float}\npoly\\.A: ${exact}\npoly\\.c: ${exact}\npoly\\.d: ${exact}\npoly\\.g: ${exact}\n$"
        "see:^see: 100 steps from 0${in_float}\nsee\\.Xp: ${exact}\nsee\\.Z: ${exact}\nsee\\.M: ${exact}\nsee\\.s: ${exact}\n$"
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
