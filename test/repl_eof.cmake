# The REPL must stop at end of input, not only at 'q'. A regression here hangs
# rather than fails, so the timeout is the assertion (MODERNIZATION.md, C21).
file(WRITE "${OUT}/repl_eof_input" "")
execute_process(COMMAND "${EXE}" INPUT_FILE "${OUT}/repl_eof_input"
                OUTPUT_QUIET TIMEOUT 10 RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the REPL did not exit on end of input: ${status}")
endif()
