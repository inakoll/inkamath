# The evaluator's benchmarks (DESIGN.md, the evaluator's speed). Each
# workload is a transcript, replayed by --check, so that a faster wrong answer
# fails rather than measures. Instructions under callgrind where valgrind is
# found, since two builds' clocks disagree for reasons of layout alone;
# otherwise, or given TIME, the best of TIME runs by the clock.
#
#   cmake -DINKAMATH=build/inkamath [-DONLY=hand] [-DTIME=5] -P bench/run.cmake
#
# 3.23 for a timestamp's microseconds.
cmake_minimum_required(VERSION 3.23)

if(NOT INKAMATH)
    message(FATAL_ERROR "set INKAMATH to the interpreter")
endif()
get_filename_component(INKAMATH "${INKAMATH}" ABSOLUTE)
find_program(VALGRIND valgrind)
# Beside the interpreter, for callgrind_annotate.
get_filename_component(callgrind_out "${INKAMATH}" DIRECTORY)
set(callgrind_out "${callgrind_out}/callgrind")

file(GLOB workloads "${CMAKE_CURRENT_LIST_DIR}/*.ink")
foreach(file IN LISTS workloads)
    get_filename_component(name "${file}" NAME_WE)
    # train.ink is data, read by the training workloads, not a transcript.
    file(STRINGS "${file}" prompts REGEX "^>> " LIMIT_COUNT 1)
    if(NOT prompts OR (ONLY AND NOT name STREQUAL ONLY))
        continue()
    endif()

    if(VALGRIND AND NOT TIME)
        execute_process(COMMAND "${VALGRIND}" --tool=callgrind
                                "--callgrind-out-file=${callgrind_out}.${name}"
                                "${INKAMATH}" --check "${file}"
                        RESULT_VARIABLE failed OUTPUT_VARIABLE out ERROR_VARIABLE err)
        string(REGEX MATCH "Collected : ([0-9]+)" collected "${err}")
        math(EXPR measure "${CMAKE_MATCH_1} / 1000000")
        set(unit "M instructions")
    else()
        if(NOT TIME)
            set(TIME 5)
        endif()
        set(measure "")
        foreach(run RANGE 1 ${TIME})
            string(TIMESTAMP start "%s%f")
            execute_process(COMMAND "${INKAMATH}" --check "${file}"
                            RESULT_VARIABLE failed OUTPUT_VARIABLE out ERROR_VARIABLE err)
            string(TIMESTAMP stop "%s%f")
            math(EXPR ms "(${stop} - ${start}) / 1000")
            if(measure STREQUAL "" OR ms LESS measure)
                set(measure ${ms})
            endif()
        endforeach()
        set(unit "ms, best of ${TIME}")
    endif()
    if(failed)
        message(SEND_ERROR "${name}: an answer moved\n${out}${err}")
        continue()
    endif()
    message("${name}\t${measure} ${unit}")
endforeach()
