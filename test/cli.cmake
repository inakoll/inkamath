# The command line, specified by running the binary: what it prints, on which
# stream, and how it exits. A transcript cannot say any of this -- it drives the
# interpreter directly and never reaches main().
#
# A case sets 'args', 'input', 'stdout', and where they are not the defaults of
# nothing and 0, 'stderr' and 'exit'. Bracket strings keep the ';' of a matrix
# from being read as a list separator.

# For CMP0054: a quoted "${x}" is its value, never the name of another variable.
cmake_minimum_required(VERSION 3.20)

function(check name)
    if(NOT DEFINED exit)
        set(exit 0)
    endif()
    file(WRITE "${OUT}/${name}.stdin" "${input}")
    execute_process(COMMAND "${EXE}" ${args}
                    WORKING_DIRECTORY "${OUT}"
                    INPUT_FILE "${OUT}/${name}.stdin"
                    OUTPUT_VARIABLE got_stdout ERROR_VARIABLE got_stderr
                    RESULT_VARIABLE got_exit TIMEOUT 10)
    if(NOT "${got_stdout}" STREQUAL "${stdout}")
        message(SEND_ERROR "${name}: stdout\n--- expected\n${stdout}--- got\n${got_stdout}---")
    endif()
    if(NOT "${got_stderr}" STREQUAL "${stderr}")
        message(SEND_ERROR "${name}: stderr\n--- expected\n${stderr}--- got\n${got_stderr}---")
    endif()
    if(NOT "${got_exit}" STREQUAL "${exit}")
        message(SEND_ERROR "${name}: exit ${got_exit}, expected ${exit}")
    endif()
    foreach(variable args input stdout stderr exit)
        unset(${variable} PARENT_SCOPE)
    endforeach()
endfunction()

# Read from a pipe, it is a filter: no banner, no prompt, one answer for each
# input, a definition answering with itself.
set(input "1+1\nsum_(k=1)^10 k\na=2\na*3\n")
set(stdout "2\n55\na=2\n6\n")
check(filter)

# A blank line and a comment are not inputs. At a terminal an empty line is
# still 'empty expression'; in a file it is only spacing.
set(input "1+1\n\n# a comment\n2*3\n")
set(stdout "2\n6\n")
check(spacing)

# An error is an answer, in its place; the exit says that one happened.
set(input "1/x\n1+1\n")
set(stdout "error: x is not defined\n2\n")
set(exit 1)
check(error)

# An unclosed bracket continues onto the next line, as at the prompt.
set(input "[1, 2;\n3, 4]\n")
set(stdout [=[
[1, 2;
 3, 4]
]=])
check(continued)

# 'q' ends the input, as at the prompt.
set(input "1\nq\n2\n")
set(stdout "1\n")
check(quit)

set(input "")
set(stdout "")
check(nothing)

# --echo prints what the recorder writes: each input after '>> ', its answer,
# a blank line. What it prints is a golden transcript.
set(args --echo)
set(input "1+1\na=2\n[1, 2;\n3, 4]\n")
set(stdout [=[
>> 1+1
2

>> a=2
a=2

>> [1, 2; 3, 4]
[1, 2;
 3, 4]

]=])
check(echo)

# ... and read back, it is the same session: a file whose first line that is
# not blank or a comment starts with '>>' is a transcript.
file(WRITE "${OUT}/echo.ink" [=[
>> 1+1
2

>> a=2
a=2

>> [1, 2; 3, 4]
[1, 2;
 3, 4]

]=])
set(args --echo echo.ink)
set(stdout [=[
>> 1+1
2

>> a=2
a=2

>> [1, 2; 3, 4]
[1, 2;
 3, 4]

]=])
check(round_trip)

# In a transcript only the '>>' lines are read. The answers it records are not
# checked -- that is what 'diff' is for.
file(WRITE "${OUT}/written.ink" "# written by hand\n\n>> 1+1\n999\n\n>> 2*3\n")
set(args written.ink)
set(stdout "2\n6\n")
check(transcript)

# Otherwise every line that is not blank or a comment is an input, whatever
# the file is called.
file(WRITE "${OUT}/plain.ink" "# plain\n1+1\n\n2*3\n")
set(args plain.ink)
set(stdout "2\n6\n")
check(plain)

# Files run in order, in one session, and standard input is left alone.
file(WRITE "${OUT}/a.txt" "a=5\n")
file(WRITE "${OUT}/b.txt" "a*2\n")
set(args a.txt b.txt)
set(input "a+1\n")
set(stdout "a=5\n10\n")
check(files)

# ... unless -i asks for it after them: the prelude, then the session.
set(args -i a.txt)
set(input "a+1\n")
set(stdout "a=5\n6\n")
check(then_input)

set(args --version)
set(stdout "inkamath 1.0.0\n")
check(version)

set(args --help)
set(stdout [=[
Usage: inkamath [options] [file...]

Runs the files in order and exits; with no file, reads standard input.
At a terminal the prompt edits the line and keeps its history.

  -i          read standard input after the files
  --echo      print each input before its answer, as a transcript
  --version   print the version and exit
  --help      print this and exit

A file whose first line that is not blank or a comment starts with '>>'
is a transcript, and only its '>>' lines are read.
]=])
check(help)

# A command line that cannot be carried out stops before anything runs.
file(WRITE "${OUT}/first.txt" "1+1\n")
set(args first.txt --frobnicate)
set(stdout "")
set(stderr "inkamath: unknown option '--frobnicate'\nTry 'inkamath --help'.\n")
set(exit 2)
check(unknown_option)

set(args first.txt missing.txt)
set(stdout "")
set(stderr "inkamath: cannot open 'missing.txt'\n")
set(exit 2)
check(missing_file)
