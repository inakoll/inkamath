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
                    RESULT_VARIABLE got_exit TIMEOUT ${CASE_TIMEOUT})
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

# The lines a file written is to hold, each found as given.
function(holds name file)
    file(READ "${OUT}/${file}" text)
    math(EXPR last "${ARGC} - 1")
    foreach(i RANGE 2 ${last})
        string(FIND "${text}" "${ARGV${i}}" at)
        if(at EQUAL -1)
            message(SEND_ERROR "${name}: ${file} does not hold\n${ARGV${i}}")
        endif()
    endforeach()
endfunction()

# As a host builds it whose flags refuse more than the checks' (C331), as C
# and as C++ (C393).
function(strict name file)
    if(NOT CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
        return()
    endif()
    string(MAKE_C_IDENTIFIER "${file}" unit)
    file(WRITE "${OUT}/${unit}.c" "#include \"${file}\"\n")
    foreach(language "C11;-std=c11" "C++17;-x;c++;-std=c++17")
        list(POP_FRONT language standard)
        execute_process(COMMAND "${CC}" ${language} -Wall -Wextra -Wpedantic -Wconversion
                                -Wshadow -Werror ${ARGN} -fsyntax-only "${unit}.c"
                        WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE failed
                        ERROR_VARIABLE errors OUTPUT_VARIABLE errors)
        if(failed)
            message(SEND_ERROR "${name}: ${file} is no strict ${standard}\n${errors}")
        endif()
    endforeach()
endfunction()

# Read by cases in every part.
set(inputs "${CMAKE_CURRENT_LIST_DIR}/compile/inputs.ink")

# A check program built and run, its report held to the one given.
function(reports name program report)
    if(NOT CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
        return()
    endif()
    execute_process(COMMAND "${CC}" -std=c11 -O2 ${program}.c -o ${program} -lm
                    WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE failed
                    ERROR_VARIABLE errors OUTPUT_VARIABLE errors)
    if(failed)
        message(SEND_ERROR "${name}: ${program}.c does not build\n${errors}")
        return()
    endif()
    execute_process(COMMAND "${OUT}/${program}" WORKING_DIRECTORY "${OUT}"
                    OUTPUT_VARIABLE got RESULT_VARIABLE code TIMEOUT ${CASE_TIMEOUT})
    if(NOT code EQUAL 0 OR NOT "${got}" STREQUAL "${report}")
        message(SEND_ERROR "${name}: exit ${code}\n--- expected\n${report}--- got\n${got}---")
    endif()
endfunction()

# A header's lines, '#' standing for a temporary's number where the rule
# does not fix it.
function(pattern out line)
    string(REPLACE "\\" "\\\\" line "${line}")
    foreach(c "^" "$" "." "|" "?" "*" "+" "(" ")" "[" "]")
        string(REPLACE "${c}" "\\${c}" line "${line}")
    endforeach()
    string(REPLACE "#" "[0-9]+" line "${line}")
    set(${out} "${line}" PARENT_SCOPE)
endfunction()
function(like name file)
    file(READ "${OUT}/${file}" text)
    math(EXPR last "${ARGC} - 1")
    foreach(i RANGE 2 ${last})
        pattern(line "${ARGV${i}}")
        if(NOT text MATCHES "${line}")
            message(SEND_ERROR "${name}: ${file} does not hold\n${ARGV${i}}")
        endif()
    endforeach()
endfunction()
function(smaller name file bytes)
    file(SIZE "${OUT}/${file}" size)
    if(NOT size LESS ${bytes})
        message(SEND_ERROR "${name}: ${file} is ${size} bytes, not under ${bytes}")
    endif()
endfunction()
# An instance of the file 'story' checked for 'count' steps: its report's
# first line matching the first pattern, then each line given found, and no
# other, its program exiting with 'exits'. A list splits at ';', which a
# line's estimate holds, so it is read as ','.
function(seqcells instance program first)
    execute_process(COMMAND "${EXE}" --check "${story}" ${instance} ${float} --steps ${count}
                            -o ${program}.c
                    WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE failed
                    ERROR_VARIABLE errors OUTPUT_VARIABLE errors TIMEOUT ${CASE_TIMEOUT})
    if(NOT failed)
        # Uncontracted, as test/CMakeLists.txt builds a check (C97).
        execute_process(COMMAND "${CC}" -std=c11 -O2 -ffp-contract=off ${program}.c
                                -o ${program} -lm
                        WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE failed
                        ERROR_VARIABLE errors OUTPUT_VARIABLE errors)
    endif()
    if(failed)
        message(SEND_ERROR "check_${program}: ${program}.c not built\n${errors}")
        return()
    endif()
    execute_process(COMMAND "${OUT}/${program}" WORKING_DIRECTORY "${OUT}"
                    OUTPUT_VARIABLE report RESULT_VARIABLE code TIMEOUT ${CASE_TIMEOUT})
    string(REPLACE ";" "," lines "${report}")
    string(REPLACE "\n" ";" lines "${lines}")
    list(REMOVE_ITEM lines "")
    list(POP_FRONT lines got)
    list(LENGTH lines given)
    list(LENGTH ARGN want)
    set(bad "")
    if(NOT code EQUAL exits OR NOT "${got}" MATCHES "^${first}$" OR NOT given EQUAL want)
        set(bad 1)
    endif()
    foreach(pattern IN LISTS ARGN)
        set(found "")
        foreach(line IN LISTS lines)
            if(line MATCHES "^${pattern}$")
                set(found 1)
            endif()
        endforeach()
        if(NOT found)
            set(bad 1)
        endif()
    endforeach()
    if(bad)
        string(REPLACE ";" "\n" patterns "${ARGN}")
        message(SEND_ERROR "check_${program}: exit ${code}\n${report}--- where\n"
                           "${first}\n${patterns}")
    endif()
endfunction()

# The cases run as nine tests under the sanitizers, the first four of about
# equal time: PART 1 up to charpoly's refusal, 2 from it, 3 from eig's, 4
# from the frequency response's, 5 and 6 compile/seqcells.ink's walks, 7 to
# 9 compile/cellloops.ink's loops, and all where PART is not given.
if(NOT PART OR PART EQUAL 1)

# Read from a pipe, it is a filter: no banner, no prompt, one answer for each
# input, a definition answering with itself.
set(input "1+1\nsum_(k=1)^10 k\na=2\na*3\n")
set(stdout "2\n55\na=2\n6\n")
check(filter)

# Each answer is written as it is given, so a long run written to a file
# shows how far it went: stopped in its second input, it has given its first.
file(WRITE "${OUT}/slow.ink" "1+1\nH[j<=80, k<=80] = 1/(j+k-1)\nsum_(j=1)^80 (H^-1)[j, 1]\n")
execute_process(COMMAND "${EXE}" slow.ink WORKING_DIRECTORY "${OUT}"
                OUTPUT_VARIABLE got_stdout ERROR_QUIET TIMEOUT 2)
if(NOT "${got_stdout}" STREQUAL "2\nH[j<=80, k<=80] = 1/(j+k-1)\n")
    message(SEND_ERROR "flushed: stdout\n--- got\n${got_stdout}---")
endif()

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

# And it ends at the end of its input, not only at 'q'. A regression here hangs
# rather than fails, so the timeout is the assertion (DESIGN.md, C21).
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

# An entry is the whole of an input, as the recorder evaluated it: an unclosed
# bracket in a transcript is the error it was, not a line to continue, and an
# entry that is only a comment is evaluated, not skipped.
file(WRITE "${OUT}/unclosed.ink" ">> [1 2\n\n>> # a comment\n\n>> 1+1\n")
set(args unclosed.ink)
set(stdout "error: missing ']' after '2'\nerror: empty expression\n2\n")
set(exit 1)
check(transcript_entry)

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
set(stdout "inkamath 2.5.0\n")
check(version)

set(args --help)
set(stdout [=[
Usage: inkamath [options] [file...]
       inkamath --compile file [model] [-o header.h]
       inkamath --check file [instance or model] -o check.c
       inkamath --check transcript.ink
       inkamath --names file

Runs the files in order and exits; with no file, reads standard input.
At a terminal the prompt edits the line and keeps its history.

  -i          read standard input after the files
  -I dir      look in dir for a file use names, after the directory of the
              file naming it; given again, after the one before
  --echo      print each input before its answer, as a transcript
  --version   print the version and exit
  --compile   write the sequences the file defines as a C header over
              doubles, named after the header: a struct, an init and a step;
              given a model the file defines, those of the model instead;
              without -o, list what would not compile, or say that it
              all compiles, and write nothing
  --check     write a C program that steps an instance the file defines,
              compiled, on the inputs the interpreter gives it, holds each
              term to the interpreter's exact one and says where one drifts;
              given a model, its instance with every default, and given
              neither, the file's own sequences; given a transcript alone,
              replay it and report each answer that is not the one recorded
  --float     with --compile or --check, write floats where they write
              doubles
  --steps n   with --check and -o, step n times rather than 100, n from
              1 to 100000
  --names     print each name the file defines, one a line, sorted, after
              what it is: model, instance, sequence, function, value or file
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

# --names lists what a file defines, sorted by name, each after what it is,
# so that a host reads no inkamath (DESIGN.md, *Hosting from Python*).
set(compiled "${CMAKE_CURRENT_LIST_DIR}/compile")
set(args --names ${compiled}/mix.ink)
set(stdout "model mix\nvalue tau\n")
check(names_mix)

set(args --names ${compiled}/pidloop.ink)
set(stdout "instance ctl\nmodel pid\nmodel plant\ninstance plt\n")
check(names_pidloop)

# A file used is a file; the prelude's names are not the file's, but one
# it defines again is, conv here.
set(args --names ${compiled}/chain.ink)
set(stdout "instance fast\nfile filters\ninstance h\nvalue k\ninstance slow\nsequence y\n")
check(names_chain)
set(args --names ${compiled}/kernel.ink)
set(stdout "model conv\nvalue lap\nmodel power\nfunction relu\nsequence u\nsequence y\n")
check(names_kernel)

# A definition by cells is a value, or a function with parameters; a
# sequence with parameters is a sequence, which no call reads; a name
# brought in is what it is in its file. Sorted as bytes, capitals first.
file(COPY ${compiled}/filters.ink DESTINATION "${OUT}")
file(WRITE "${OUT}/kinds.ink" [=[
use filters (lowpass)
E[j<=2, k<=2] = j*k
f(x)[j<=2] = x*j
t(x)_0 = 1
t(x)_k = x*t(x)_(k-1)
w = lowpass(u_n = n)
z_n = w.v_n
]=])
set(args --names kinds.ink)
set(stdout "value E\nfunction f\nfile filters\nmodel lowpass\nsequence t\ninstance w\nsequence z\n")
check(names_kinds)

# A transcript's entries are its definitions; a file of none lists none.
file(WRITE "${OUT}/session.ink" ">> f(x) = x\nf(x) = x\n\n>> a = 1\na = 1\n")
set(args --names session.ink)
set(stdout "value a\nfunction f\n")
check(names_transcript)
file(WRITE "${OUT}/empty.ink" "# nothing\n")
set(args --names empty.ink)
set(stdout "")
check(names_empty)

# Refused as --compile refuses: a file that cannot be read, and one whose
# line fails, which lists nothing.
set(args --names missing.ink)
set(stderr "inkamath: cannot open 'missing.ink'\n")
set(exit 2)
check(names_missing)
file(WRITE "${OUT}/bad.ink" "a = 1\nb = 1+\n")
set(args --names bad.ink)
set(stderr "inkamath: bad.ink: b = 1+: unexpected end of input after '+'\n")
set(exit 1)
check(names_failing)

# One file, and nothing else.
file(COPY ${compiled}/mix.ink DESTINATION "${OUT}")
foreach(given "" "mix.ink;mix.ink" "mix.ink;-i" "mix.ink;--echo" "mix.ink;-o;x.h")
    set(args --names ${given})
    set(stderr "inkamath: --names takes a file and -I directories, nothing else\nTry 'inkamath --help'.\n")
    set(exit 2)
    string(MAKE_C_IDENTIFIER "names_usage_${given}" name)
    check(${name})
endforeach()

# -I dir (DESIGN.md), where use looks after the using file's own directory,
# in every mode, the fixtures in compile/search, app/model.ink's comment
# giving its values.
set(dir "${CMAKE_CURRENT_LIST_DIR}/compile/search")
set(model "${dir}/app/model.ink")
set(search -I "${dir}/one" -I "${dir}/two")
# Beside the file first, then each -I in the command line's order; a
# file found through -I looks beside itself first, so tools reads two's
# shared where the session reads one's.
set(head "use gains\nuse shared\nuse tools\nv = [gains.k, shared.s, tools.t, tools.w]
gain(c = 0) = { ... }\ng = gain()\n")
set(args ${search} "${model}" -i)
set(input "v\n")
set(stdout "${head}[1, 2, 3, 20]\n")
check(search_order)
set(args -I "${dir}/two" -I "${dir}/one" "${model}" -i)
set(input "v\n")
set(stdout "${head}[1, 20, 3, 20]\n")
check(search_reversed)

# At the prompt the session's own directory is the working one, searched
# first: unit here, shared from one.
file(WRITE "${OUT}/unit.ink" "u = 7\n")
set(args -I "${dir}/one")
set(input "use unit\nuse shared\n[unit.u, shared.s]\n")
set(stdout "use unit\nuse shared\n[7, 2]\n")
check(search_prompt)

# --compile, --check and --names take it too, before or after the file.
set(args --compile "${model}" gain ${search})
set(stdout "gain in model.ink compiles\n")
check(search_compile)
set(args ${search} --check "${model}" g -o g.c)
check(search_check)
reports(search_check g "g: 100 steps from 0, against exact values\ng.y: within 0\n")
# deep's probe, in two, takes one's gains, not app's: y has its clause.
set(args --check "${dir}/app/deep.ink" deep -o deep.c ${search})
check(search_check_nested)
reports(search_check_nested deep "deep: 100 steps from 0, against exact values\ndeep.y: within 0\n")
# A transcript's replay, beside it the working directory here.
file(WRITE "${OUT}/replay.ink" ">> use shared\nuse shared\n\n>> shared.s\n2\n")
set(args --check replay.ink -I "${dir}/one")
set(stdout "replay.ink: 2 answers, each as recorded\n")
check(search_replay)
set(args --names "${model}" ${search})
set(stdout "instance g\nmodel gain\nfile gains\nfile shared\nfile tools\nvalue v\n")
check(search_names)
# --names takes a file and -I alone, in these words, which names_usage's
# five cases above take too.
set(args --names "${model}" -o x.h ${search})
set(stderr "inkamath: --names takes a file and -I directories, nothing else\nTry 'inkamath --help'.\n")
set(exit 2)
check(search_names_usage)

# A file found nowhere names the -I directories, in order; a cycle
# through -I is refused as one beside is, a file being one file by
# whatever route it is reached.
set(args ${search} "${dir}/app/lost.ink")
set(stdout "error: cannot read nowhere.ink, nor find it in ${dir}/one or ${dir}/two\n")
set(exit 1)
check(search_lost)
set(args ${search} -I "${dir}" "${dir}/app/lost.ink")
set(stdout "error: cannot read nowhere.ink, nor find it in ${dir}/one, ${dir}/two or ${dir}\n")
set(exit 1)
check(search_lost_three)
# A relative directory is the working directory's, and named as given.
file(RELATIVE_PATH one "${OUT}" "${dir}/one")
file(RELATIVE_PATH two "${OUT}" "${dir}/two")
set(args -I "${one}" -I "${two}" "${model}" -i)
set(input "v\n")
set(stdout "${head}[1, 2, 3, 20]\n")
check(search_relative)
set(args -I "${one}" "${dir}/app/lost.ink")
set(stdout "error: cannot read nowhere.ink, nor find it in ${one}\n")
set(exit 1)
check(search_relative_lost)
set(args ${search} "${dir}/app/loop.ink")
set(stdout "error: ring.ink, line 2: hub.ink, line 2: ring.ink uses itself\n")
set(exit 1)
check(search_cycle)
# A directory named as the file is found, beside or in an -I, and cannot
# be read, as on Windows (C437): neither an empty file nor passed over.
file(MAKE_DIRECTORY "${OUT}/hollow/hole.ink")
file(WRITE "${OUT}/hollow/main.ink" "use hole\n")
file(WRITE "${OUT}/filled/hole.ink" "h = 1\n")
set(args -I filled hollow/main.ink)
set(stdout "error: cannot read hole.ink\n")
set(exit 1)
check(search_hollow_beside)
# Found in an -I, it is named where it was found, the directory as given.
file(TO_NATIVE_PATH "hollow/hole.ink" hole)
set(args -I hollow -I filled)
set(input "use hole\n")
set(stdout "error: cannot read ${hole}\n")
set(exit 1)
check(search_hollow_first)
set(args -I "${dir}/one" -I hollow -I filled)
set(input "use hole\n")
set(stdout "error: cannot read ${hole}\n")
set(exit 1)
check(search_hollow_later)

# A directory, after its own -I, once: each checked where the command
# line gives it, as --steps is, so the first wrong in its order is said,
# and before anything runs.
set(args -I)
set(stderr "inkamath: -I takes a directory\nTry 'inkamath --help'.\n")
set(exit 2)
check(search_bare)
set(args -I "${dir}/none" "${model}")
set(stderr "inkamath: -I ${dir}/none is not a directory\n")
set(exit 2)
check(search_missing)
set(args -I "${model}" "${model}")
set(stderr "inkamath: -I ${model} is not a directory\n")
set(exit 2)
check(search_file)
set(args -I "${dir}/one" -I "${dir}/two/../one" "${model}")
set(stderr "inkamath: -I ${dir}/two/../one is given twice\nTry 'inkamath --help'.\n")
set(exit 2)
check(search_twice)
set(args -I "${dir}/none" -I)
set(stderr "inkamath: -I ${dir}/none is not a directory\n")
set(exit 2)
check(search_first)
set(args -I -I "${dir}/one")
set(stderr "inkamath: -I -I is not a directory\n")
set(exit 2)
check(search_dashed)
set(args "-I${dir}/one" "${model}")
set(stderr "inkamath: unknown option '-I${dir}/one'\nTry 'inkamath --help'.\n")
set(exit 2)
check(search_joined)


# What the compiler cannot express it refuses by name, and writes nothing
# (DESIGN.md, phase 14, step 2).
file(WRITE "${OUT}/model.ink" "a_0 = 1\na_n = a_(n-1) + i*n\n")
file(REMOVE "${OUT}/model.h")
set(args --compile model.ink -o model.h)
set(stderr "inkamath: cannot compile a: a complex number\n")
set(exit 1)
check(compile_refused)
if(EXISTS "${OUT}/model.h")
    message(SEND_ERROR "compile_refused: model.h was written")
endif()

set(args --compile model.ink a b)
set(stderr "inkamath: --compile takes a file, optionally a model it defines, and -o header.h\nTry 'inkamath --help'.\n")
set(exit 2)
check(compile_usage)

# Without -o, every definition that would not compile, and why, and nothing
# written: one that reads a refused definition is refused in turn.
file(WRITE "${OUT}/refused.ink" "sq(x) = x^2\na_0 = 1\na_n = a_(n-1) + i*n\nb_n = a_n + 1\nc_n = sq(u_n)\ns(g = 1, u_n) = { v_0 = 0\n v_n = g*v_(n-1) + u_n }\nd_n = s(g = n, u_n = c_n).v_n\n")
set(args --compile refused.ink)
set(stdout "cannot compile a: a complex number\ncannot compile b: a, which cannot be compiled\ncannot compile d: an instance of s, which keeps a history, made anew at each step\n")
set(stderr "")
set(exit 1)
check(compile_refusals)

# Two sizes neither stretches over are named, and the cell form suggested.
file(WRITE "${OUT}/unmet.ink" "y_n = [n n] + [1 2 3]\n")
set(args --compile unmet.ink)
set(stdout "cannot compile y: a 1x2 matrix and a 1x3 matrix have different sizes; write it by its cells, as A[i,j] + c[1,j]\n")
set(exit 1)
check(compile_unmet)

# A factorial of a constant is one, as a series is written.
file(WRITE "${OUT}/series.ink" "x_n = n/4\ny_n = sum_(q=0)^4 x_n^q/!q\n")
set(args --compile series.ink -o series.h)
check(compile_factorial)
holds(compile_factorial series.h " / 6.0 + pow(m_->x[0], 4.0) / 24.0;\n")

# Several rates (DESIGN.md): a read at a period the sequence is not computed
# at, one before its term is computed, and one at an index of neither form.
file(WRITE "${OUT}/rates.ink" "y_0 = 0\ny_m = x_(2*m + 1)\nz_n = y_(floor(n/2))\nv_n = y_(floor(n/3))\nu_m = x_(m*m)\n")
set(args --compile rates.ink)
set(stdout "cannot compile u: x_(...): an index other than a whole multiple of m plus a constant\ncannot compile v: y_(...): read every 3 steps, and y is computed every 2\ncannot compile z: y_(...): read before it is computed; read the term before it\n")
set(stderr "")
set(exit 1)
check(compile_rates_refused)

# C119: a term read where the clause's index is not seen, under a grad that
# takes its name, says so rather than naming no index.
file(WRITE "${OUT}/c119.ink" "f(t) = x_t\ny_n = f(n)\nz_n = grad_(n = 2) n*x_n\n")
set(args --compile c119.ink)
set(stdout "cannot compile z: x_(...): a term read where z's index is not seen\n")
set(exit 1)
check(compile_c119)

# What several rates left refused (DESIGN.md): a slow sequence read back at
# the input's rate, and a hold of a term the step never computes where the
# interpreter could give one, before a slow sequence's first tick or below a
# guarded one's base clauses.
file(WRITE "${OUT}/slow.ink" "k_n = n/8\ny_m = k_(2*m + 1)\nc_n = y_(n-1)\nz_n = y_(floor((n - 1)/2))\n")
set(args --compile slow.ink)
set(stdout "cannot compile c: y_(...): read every step, and y is computed every 2\ncannot compile z: z_0 reads y_-1, before y's first tick, where its samples could give a term\n")
set(exit 1)
check(compile_slow_refused)

# A sample read from a window that cannot hold the term the interpreter has.
file(WRITE "${OUT}/window.ink" "k_n = n/8\ny_m = k_(2*m - 1) + k_(2*m)\n")
set(args --compile window.ink)
set(stdout "cannot compile y: y_0 reads k_-1, before the step computes k, where its clauses could give a term\n")
set(exit 1)
check(compile_slow_window_refused)

file(WRITE "${OUT}/below.ink" "u_0 = 0\nu_m | x_(2*m) > 1 = 1\nu_m = x_(2*m)\nw_n = u_(floor(n/2) - 1)\n")
set(args --compile below.ink)
set(stdout "cannot compile w: w_0 reads u_-1, below u's base clauses, where only its guards could give a term\n")
set(exit 1)
check(compile_slow_below_refused)

# A hold on the right of 'and' or 'or', which is read only where the left has
# not decided; it was said to be where a term is computed again (C378).
file(WRITE "${OUT}/right.ink" "s_m = x_(10*m)\ny_n | x_n > 0 and s_(floor(n/10)) > 0 = 1\ny_n = 0\n")
set(args --compile right.ink)
set(stdout "cannot compile y: s_(...): a hold on the right of 'and' or 'or'\n")
set(exit 1)
check(compile_slow_right_refused)

# A ratio that is not whole; a slow sequence read by another, which a hold at
# the input's rate sampled says; and a hold at another period, which no hold
# says.
file(WRITE "${OUT}/three.ink" "u_0 = 0\nu_m = x_(2*m)\nt_m = x_(3*m)\np_k = x_(4*k)\nq_n = t_(floor(2*n/3))\nv_k = u_(2*k)\nr_m = x_(2*m) - p_(floor(m/2))\nw_k = x_(4*k) - u_(floor(k/2))\n")
set(args --compile three.ink)
set(stdout "cannot compile q: t_(...): an index other than a whole multiple of n plus a constant\ncannot compile r: p_(...): one sequence at another rate read by another; hold p at the input's rate and sample the hold\ncannot compile v: u_(...): one sequence at another rate read by another; hold u at the input's rate and sample the hold\ncannot compile w: u_(...): read every 8 steps, and u is computed every 2\n")
set(exit 1)
check(compile_slow_read_refused)

# C74: a hold whose ticks are not one period of the sequence it holds names
# a term further behind at each tick, which no window holds.
file(WRITE "${OUT}/c74.ink" "u_0 = 0\nu_m = x_(2*m)\ns_0 = 0\ns_k = x_(4*k) - u_(floor(k/2))\n")
set(args --compile c74.ink)
set(stdout "cannot compile s: u_(...): read every 8 steps, and u is computed every 2\n")
set(exit 1)
check(compile_c74)

# C233: a slow sequence read back by another as slow said "read every step".
file(WRITE "${OUT}/c233.ink" "q_0 = 0\nq_m = q_(m-1) + x_(2*m)\nw_0 = 0\nw_m = w_(m-1) + q_(m-1) + x_(2*m)\n")
set(args --compile c233.ink)
set(stdout "cannot compile w: q_(...): one sequence at another rate read by another; hold q at the input's rate and sample the hold\n")
set(exit 1)
check(compile_c233)

# C234: as is one on the right of an 'and', which compiled to a wrong term.
file(WRITE "${OUT}/c234.ink" "y_0 = 0\ny_m = x_(2*m)\nc_n | x_n > 0 and y_(n-1) > 0 = n\nc_n = 0\n")
set(args --compile c234.ink)
set(stdout "cannot compile c: y_(...): read every step, and y is computed every 2\n")
set(exit 1)
check(compile_c234)

# C86: a clause the interpreter refuses wherever it is taken is what a step
# says there, NaN, so a model whose guard never takes it compiles, as log
# does, its '| x <= 0 = 1/0' NaN in the header's function for it.
file(WRITE "${OUT}/c86.ink" "gd(a = 2) = {\n    h(x) = 1\n    h(x) | x <= 0 = 1/0\n    y_n = h(a + n)\n}\n")
set(args --compile c86.ink gd)
set(stdout "gd in c86.ink compiles\n")
check(compile_c86)

file(WRITE "${OUT}/logged.ink" "lg(a = 2) = {\n    y_n = log(a + n)\n}\n")
set(args --compile logged.ink lg)
set(stdout "lg in logged.ink compiles\n")
check(compile_log)

# A base term is computed at its own index, so it reads a term from there
# back, and only where that term's sequence has started.
file(WRITE "${OUT}/seeds.ink" "y_0 = x_1\ny_n = y_(n-1) + x_n\na_1 = 1\na_n = a_(n-1) + 1\nb_0 = a_0\nb_n = b_(n-1)\n")
set(args --compile seeds.ink)
set(stdout "cannot compile b: b_0 reads a_0, before it starts at 1\ncannot compile y: x_(...): a term after the one being computed\n")
set(stderr "")
set(exit 1)
check(compile_base_reads_refused)

# C122: a cell of one term outside its size is refused in the interpreter's
# words, where the compiler wrote past the term's cells.
file(WRITE "${OUT}/c122.ink" "y_n[j<=2] = n\ny_1[3] = 5\n")
set(args --compile c122.ink)
set(stdout "cannot compile y: row 3, column 1 is outside a 2x1 matrix\n")
set(exit 1)
check(compile_c122)

# C125: a clause for one cell of a value written whole names as many indices
# as reading a cell of it takes, as the interpreter asks, where the compiler
# took a slice where none was named and dropped one that was.
file(WRITE "${OUT}/c125.ink" "A = [1 2; 3 4]\nA[1,1,2] = 9\nv_n = n*A\ny_0 = [1 2;; 3 4]\n"
     "y_n = 2*y_(n-1)\ny_0[1,2] = 5\nz_n = [1 2; 3 4]\nz_1[1,1,2] = 5\nw_n = [1 2;; 3 4]\n"
     "w_n[1,2] = 5\n")
set(args --compile c125.ink)
set(stdout "cannot compile v: a clause for one cell of A, a 2x2 matrix, names no slice
cannot compile w: a clause for one cell of w_n, a 2x1x2 tensor, names its slice, row and column
cannot compile y: a clause for one cell of y_0, a 2x1x2 tensor, names its slice, row and column
cannot compile z: a clause for one cell of z_1, a 2x2 matrix, names no slice
")
set(exit 1)
check(compile_c125)

# C126: a cell of every term meets a base term's own cell at its slice too,
# and is named by it, where the compiler took y_0[1,1,1] for y_0[2,1,1].
file(WRITE "${OUT}/c126.ink" "y_0 = [1 2;; 3 4]\ny_0[1,1,1] = 7\ny_n = 2*y_(n-1)\ny_n[2,1,1] = n\n"
     "z_0 = [1 2;; 3 4]\nz_n = 2*z_(n-1)\nz_n[2,1,1] = n\n")
set(args --compile c126.ink)
set(stdout "cannot compile y: y_0 and y_n[2,1,1] both give a cell of y_0; write y_0[2,1,1] to say which
cannot compile z: z_0 and z_n[2,1,1] both give a cell of z_0; write z_0[2,1,1] to say which
")
set(exit 1)
check(compile_c126)

# C127: every clause for one cell is held to the size, as the interpreter
# holds it, not only those met before the term's cells are all given.
file(WRITE "${OUT}/c127.ink" "y_0 = [1 2]\ny_0[1,1] = 3\ny_0[1,2] = 4\ny_0[1,3] = 5\ny_n = y_(n-1)/2\n")
set(args --compile c127.ink)
set(stdout "cannot compile y: row 1, column 3 is outside a 1x2 matrix\n")
set(exit 1)
check(compile_c127)

# What tensors compiled refuses (DESIGN.md, test/compile/tensor.ink), in the
# interpreter's words where it has them; y and P compile.
file(WRITE "${OUT}/tensor.ink" "T = [1 2; 3 4;; 5 6; 7 8]\nU = [1 2; 3 4;; 5 6; 7 8;; 9 10; 11 12]\n"
     "a_n = (n*T)[2,1]\nb_n = (n*T)^2\nc_n | n*T > 1 = 1\nc_n = 0\nd_n = n*T + U\n"
     "f_n = [n 1;; 2 3 4]\ng_n = lim p(n*T)\n"
     "m_n[b<=2, i<=1, j<=1] | n > 2 = b\nm_n[b<=2, i<=1, j<=1] = 0\np(A)_0 = A\n"
     "p(A)_k = p(A)_(k-1)/2\nq_n = [n;; 1] + [1 2]\nr_n = [n;; 1]*[1 2; 3 4]\n"
     "y_0 = 0\ny_n = y_(n-1) + T[2,1,2]\nP[b<=2, j<=2, k<=2] = b\n")
set(args --compile tensor.ink)
set(stdout "cannot compile a: a 2x2x2 tensor takes one index or three, not two
cannot compile b: only a matrix has a power, not a 2x2x2 tensor
cannot compile c: a comparison needs single values, not a 2x2x2 tensor
cannot compile d: a 2x2x2 tensor and a 3x2x2 tensor have different numbers of slices
cannot compile f: the slices of a tensor have one size, not 1x2 and 1x3
cannot compile g: a tensor in a limit, for now
cannot compile q: a tensor whose slices are single values met by a matrix, for now
cannot compile r: a tensor whose slices are single values met by a matrix, for now
")
set(exit 1)
check(compile_tensor_refused)

# Two matrices compared whole (DESIGN.md, test/compile/orthogonal.ink): equal
# where every cell is, NaN where a cell is in a header that writes NaN, and
# an order of matrices or two shapes refused, constant or not.
file(WRITE "${OUT}/same.ink" "r(v) | v > 0 = v\nsame(x_n[j<=2]) = {\n"
     "    e_n = x_n == [1; 2]\n    u_n = x_n <> [1; 2]\n    z_n = r(x_n[1])\n}\n")
set(args --compile same.ink same -o same.h)
check(compile_mateq)
set(meet "(m_->x[0][0][0] == 1.0 && m_->x[0][1][0] == 2.0) ? 1.0 : 0.0);")
holds(compile_mateq same.h "_ = isnan(m_->x[0][0][0]) || isnan(m_->x[0][1][0]);"
      "_ ? NAN : ${meet}" "_ ? NAN : !${meet}")
file(WRITE "${OUT}/apart.ink" "c_n = [n, 1] < [2, 2]\nd_n = [n, 1] == 0\n"
     "f_n = [n, 1] == [n; 1]\ng_n = n*([1, 1] == [1; 1])\nh_n = n*([1, 1] == 1)\n")
set(args --compile apart.ink)
string(CONCAT stdout "cannot compile c: a comparison needs single values, not a 1x2 matrix\n"
       "cannot compile d: a 1x2 matrix compared with a single value, never equal; "
       "compare with 0*M, M's zero matrix, or a cell, M[j,k]\n"
       "cannot compile f: a 1x2 matrix compared with a 2x1 matrix, never equal\n"
       "cannot compile g: a 1x2 matrix compared with a 2x1 matrix, never equal\n"
       "cannot compile h: a 1x2 matrix compared with a single value, never equal; "
       "compare with 0*M, M's zero matrix, or a cell, M[j,k]\n")
set(exit 1)
check(compile_mateq_refused)

# A gradient with respect to a tensor compiled refuses a Jacobian, as the
# interpreter does, in its words (test/compile/tensorgrad.ink).
file(WRITE "${OUT}/tensorgrad.ink" "T = [1 2; 3 4;; 5 6; 7 8]\nh_n = grad_(V = n*T) 2*V\n"
     "k_n = grad_(V = n*T) V[1]\nw_n = grad_(v = [n 1]) v*T\n")
set(args --compile tensorgrad.ink)
set(stdout "cannot compile h: grad of a tensor with respect to a tensor is a Jacobian, which it does not give
cannot compile k: grad of a matrix with respect to a tensor is a Jacobian, which it does not give
cannot compile w: grad of a tensor with respect to a matrix is a Jacobian, which it does not give
")
set(exit 1)
check(compile_tensorgrad_refused)

# C313: under grad, an empty sum's local hides nothing, so the global t read
# after it is searched as the interpreter's is, and refused in its words.
file(WRITE "${OUT}/c313.ink" "x = 100\nt = x^2\ny_n = grad_(x = 2) ((sum_(k=1)^0 (t = k*x)) + t)\n")
set(args --compile c313.ink)
set(stdout "cannot compile y: t reads the global x, which grad's x does not reach\n")
set(stderr "")
set(exit 1)
check(compile_c313)

# Locals compiled (DESIGN.md): each model of compile/locals.ink compiled
# beside its twin, rk's step a temporary per stage, an instance of each
# checked to its twin's report but for the twin's stages, and the refusals in
# their words.
set(story "${CMAKE_CURRENT_LIST_DIR}/compile/locals.ink")
foreach(model IN ITEMS rk osc twice across root sens)
    foreach(written IN ITEMS ${model} staged${model})
        set(args --compile ${story} ${written} -o ${written}.h)
        check(compile_locals_${written})
    endforeach()
    strict(compile_locals_${model} ${model}.h)
endforeach()
holds(compile_locals_rk rk.h
      "    const double t0_ = 0.0 - m_->x[1];\n"
      "    const double t1_ = 0.0 - (m_->x[1] + m_->h / 2.0 * t0_);\n"
      "    const double t2_ = 0.0 - (m_->x[1] + m_->h / 2.0 * t1_);\n"
      "    const double t3_ = 0.0 - (m_->x[1] + m_->h * t2_);\n"
      "    m_->x[0] = m_->index_ == 0 ? 1.0 : m_->x[1] + m_->h / 6.0 * (t0_ + 2.0 * t1_ + 2.0 * t2_ + t3_);\n")
file(READ "${OUT}/rk.h" header)
if(header MATCHES "k[1-4]")
    message(SEND_ERROR "compile_locals_rk: rk.h keeps a stage\n${header}")
endif()

set(args --compile ${story} refused)
set(stdout "cannot compile a: a local by cells, for now
cannot compile b: t, bound on the right of 'and' or 'or', read after it, for now
cannot compile c: t, carried past a term of a sum without an upper bound, for now
cannot compile d: t is not defined
cannot compile f: t, carried past a term of a sum without an upper bound, for now
cannot compile g: k is the sum's index, so a local cannot define it
cannot compile h: x is grad's variable, so a local cannot define it
cannot compile i: a local cannot take parameters
cannot compile j: t, bound in a term's index, for now
cannot compile k: grad cannot differentiate a power whose exponent changes with x, unless its base is e
cannot compile l: a local cannot take an index
")
set(exit 1)
check(compile_locals_refused)

if(CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
    foreach(instance IN ITEMS r o tw ac rt se)
        foreach(written IN ITEMS ${instance} staged${instance})
            set(args --check ${story} ${written} -o ${written}.c)
            check(check_locals_${written})
            execute_process(COMMAND "${CC}" -std=c11 -O2 ${written}.c -o ${written} -lm
                            WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE failed
                            ERROR_VARIABLE errors OUTPUT_VARIABLE errors)
            if(failed)
                message(SEND_ERROR "check_locals_${written}: ${written}.c does not build\n${errors}")
            endif()
            execute_process(COMMAND "${OUT}/${written}" WORKING_DIRECTORY "${OUT}"
                            OUTPUT_VARIABLE report_${written} RESULT_VARIABLE code_${written}
                            TIMEOUT ${CASE_TIMEOUT})
        endforeach()
        # The twin's stages are sequences of its own, and an estimate's
        # digits the disturbed runs', which differ where the operations do.
        string(REPLACE "staged${instance}" "${instance}" staged "${report_staged${instance}}")
        string(REGEX REPLACE "\n${instance}\\.k[1-4]: [^\n]*" "" staged "${staged}")
        foreach(report staged report_${instance})
            string(REGEX REPLACE "about [0-9.e+-]+ from" "about <e> from" ${report} "${${report}}")
        endforeach()
        if(NOT code_${instance} EQUAL 0 OR NOT code_staged${instance} EQUAL 0 OR
           NOT "${report_${instance}}" STREQUAL "${staged}" OR
           NOT "${staged}" MATCHES "^${instance}: 100 steps from 0, against ")
            message(SEND_ERROR "check_locals_${instance}: exit ${code_${instance}}\n${report_${instance}}--- where staged${instance}'s, exit ${code_staged${instance}}, is\n${staged}")
        endif()
    endforeach()
    # Exact in doubles, as worked by hand in the story.
    set(exact_tw "^tw: 100 steps from 0, against exact values\ntw\\.a: within 0\ntw\\.v: within 0\ntw\\.w: within 0\ntw\\.y: within 0\ntw\\.z: within 0\n$")
    set(exact_ac "^ac: 100 steps from 0, against exact values\nac\\.e: within 0\nac\\.g: within 0\nac\\.y: within 0\n$")
    set(exact_se "\nse\\.e: within 0\nse\\.j: within 0\nse\\.m: within 0\nse\\.o: within 0\nse\\.p: within 0\nse\\.q: within 0\nse\\.r: within 0\nse\\.s: within 0\nse\\.v: within 0\n")
    foreach(instance IN ITEMS tw ac se)
        if(NOT "${report_${instance}}" MATCHES "${exact_${instance}}")
            message(SEND_ERROR "check_locals_${instance}: not within 0 as worked by hand\n${report_${instance}}")
        endif()
    endforeach()
endif()

# The prelude's mod refuses a matrix to divide by, as the interpreter does,
# rather than compile the product of matrices it would be.
file(WRITE "${OUT}/mod.ink" "y_n = [1 1]*mod(n, [2 3; 5 11])*[1; 1]\n")
set(args --compile mod.ink)
set(stdout "cannot compile y: mod needs a single value to divide by, not a 2x2 matrix; write it by its cells\n")
set(exit 1)
check(compile_mod_refused)

set(args --compile model.ink)
file(WRITE "${OUT}/model.ink" "a_0 = 1\na_n = a_(n-1) + n\n")
set(stdout "model.ink compiles\n")
set(exit 0)
check(compile_nothing_refused)

# C415: a file with no sequence lists none in its header's comment.
file(WRITE "${OUT}/nothing.ink" "x = 1\n")
set(args --compile nothing.ink -o nothing.h)
check(compile_no_sequence)
holds(compile_no_sequence nothing.h " * A step takes no input.\n */\n")

# A file of models alone is compiled by a model's name.
file(WRITE "${OUT}/gains.ink" "gain(k = 2, x_n) = {\n    y_n = k*x_n\n}\n")
set(args --compile gains.ink)
set(stdout "gains.ink defines no sequence of its own; compile a model by name\n")
check(compile_models_only)

# A model compiled by name declares its inputs, so a name nothing defines is a
# mistake rather than one more input (DESIGN.md, phase 15).
file(WRITE "${OUT}/models.ink" "gain(k = 2, x_n) = { y_n = k*x_n + z_n }\n")
set(args --compile models.ink gain -o gain.h)
set(stderr "inkamath: cannot compile y: z is not defined\n")
set(exit 1)
check(compile_undeclared)

set(args --compile models.ink loss -o gain.h)
set(stderr "inkamath: models.ink defines no model loss\n")
set(exit 1)
check(compile_no_model)

# A default reading another instance's term is refused with its remedy.
file(WRITE "${OUT}/trained.ink" "trainer(a = 1/2) = {\n    W_0 = 0\n    W_n = W_(n-1) + a*(1 - W_(n-1))\n}\n"
     "train = trainer()\nuser(W = train.W_10, y_n) = {\n    u_n = W*y_n\n}\n")
set(args --compile trained.ink user)
set(stdout "cannot compile W: trained.train.W_...: a term read outside a general clause; write its value, or pass it as a parameter\ncannot compile u: W, which cannot be compiled\n")
set(exit 1)
check(compile_default_term)

# An instance checked: the C program its check writes is built and run by the
# tests, beside the compiled models.
set(args --check models.ink loss -o loss.c)
set(stderr "inkamath: models.ink defines no instance or model loss\n")
set(exit 1)
check(check_no_instance)

# A model is checked as its instance with every default is, but not one that
# needs data to step.
set(args --check gains.ink gain -o gain.c)
set(stderr "inkamath: gain in gains.ink takes x, which has no default; define an instance that gives it\n")
set(exit 1)
check(check_model_undefaulted)

# Without an instance, the file's own sequences, as --compile without a model
# compiles them, if the interpreter can give them all they read.
set(args --check gains.ink -o gains.c)
set(stderr "inkamath: gains.ink defines no sequence of its own; check a model by name\n")
set(exit 1)
check(check_models_only)
# C420: a file of no model is not told to check one.
file(WRITE "${OUT}/constant.ink" "x = 1\n")
set(args --check constant.ink -o constant.c)
set(stderr "inkamath: constant.ink defines no sequence\n")
set(exit 1)
check(check_no_sequence)

file(WRITE "${OUT}/fed.ink" "y_0 = 0\ny_n = y_(n-1) + x_n\n")
set(args --check fed.ink -o fed.c)
set(stderr "inkamath: fed.ink reads x, which nothing defines; define it\n")
set(exit 1)
check(check_undefined_input)
# C419: and so is a name read as a constant, which the header leaves to its
# host, NaN, and the check left NaN, comparing nothing.
file(WRITE "${OUT}/free.ink" "y_n = k + n\n")
set(args --check free.ink -o free.c)
set(stderr "inkamath: free.ink reads k, which nothing defines; define it\n")
set(exit 1)
check(check_undefined_constant)

# C418: the report names the file as it is spelt, not as a printf format.
file(WRITE "${OUT}/50%.ink" "fib_0 = 0\nfib_1 = 1\nfib_(n+2) = fib_(n+1) + fib_n\n")
set(args --check 50%.ink --steps 3 -o percent.c)
check(check_file_percent)
if(CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
    execute_process(COMMAND "${CC}" -std=c11 percent.c -o percent -lm WORKING_DIRECTORY "${OUT}")
    execute_process(COMMAND "${OUT}/percent" OUTPUT_VARIABLE report TIMEOUT ${CASE_TIMEOUT})
    if(NOT report STREQUAL "50%.ink: 3 steps from 0, against exact values\nfib: within 0\n")
        message(SEND_ERROR "check_file_percent: reports\n${report}")
    endif()
endif()

set(args --check models.ink gain)
set(stderr "inkamath: --check takes a transcript, or a file, optionally an instance or a model it defines, and -o check.c\nTry 'inkamath --help'.\n")
set(exit 2)
check(check_usage)

# --steps n: --check steps the instance n times where it stepped it a hundred.
file(WRITE "${OUT}/coast.ink" "fall(N = 1000, T = 125, g = 1, v0 = 64) = {\n    h = T/N\n    v_0 = v0\n    v_n = v_(n-1) - g*h\n    y_0 = 0\n    y_n = y_(n-1) + h*v_n\n}\ncoast = fall()\n")

# The program steps the instance n times and says so first, anywhere the
# option is given on the line.
set(args --check coast.ink coast --steps 1001 -o coast.c)
check(check_steps)
holds(check_steps coast.c
    [[printf("coast: 1001 steps from 0, against exact values\n");]]
    "    for (int n_ = 0; n_ < 1001; ++n_) {\n"
    "static const double want_0[1001] = {")
set(args --steps 1001 --check coast.ink coast -o first.c)
check(check_steps_first)
holds(check_steps_first first.c [[printf("coast: 1001 steps from 0, against exact values\n");]])
set(args --check coast.ink coast --float --steps 1001 -o floated.c)
check(check_steps_float)
holds(check_steps_float floated.c
    [[printf("coast: 1001 steps from 0 in float, against exact values\n");]])
set(args --check coast.ink coast --steps 1 -o one.c)
check(check_steps_one)
holds(check_steps_one one.c [[printf("coast: 1 step from 0, against exact values\n");]])

# A hundred is the default, byte for byte.
file(MAKE_DIRECTORY "${OUT}/plain" "${OUT}/hundred")
set(args --check coast.ink coast -o plain/coast.c)
check(check_steps_default)
set(args --check coast.ink coast --steps 100 -o hundred/coast.c)
check(check_steps_hundred)
file(READ "${OUT}/plain/coast.c" plain)
set(hundred "")
if(EXISTS "${OUT}/hundred/coast.c")
    file(READ "${OUT}/hundred/coast.c" hundred)
endif()
if(NOT "${plain}" STREQUAL "${hundred}")
    message(SEND_ERROR "check_steps_hundred: --steps 100 is not the program without it")
endif()

# The largest is read: the instance is refused after it, before any term.
set(args --check coast.ink nothing --steps 100000 -o nothing.c)
set(stderr "inkamath: coast.ink defines no instance or model nothing\n")
set(exit 1)
check(check_steps_most)

# Anything but a whole number from 1 to 100000 in decimal digits is refused
# in one sentence, a form that adds nothing among them, before anything runs.
set(i 0)
foreach(n IN ITEMS 0 -5 2.5 1e3 100001 0100 +5 99999999999999999999)
    math(EXPR i "${i} + 1")
    set(args --check coast.ink coast --steps ${n} -o refused.c)
    set(stderr "inkamath: --steps takes a whole number from 1 to 100000\nTry 'inkamath --help'.\n")
    set(exit 2)
    check(check_steps_refused_${i})
endforeach()
set(args --check coast.ink coast -o refused.c --steps)
set(stderr "inkamath: --steps takes a whole number from 1 to 100000\nTry 'inkamath --help'.\n")
set(exit 2)
check(check_steps_missing)
# So is -o last, where it was an unknown option (C280).
set(args --check coast.ink coast -o)
set(stderr "inkamath: -o takes a file to write\nTry 'inkamath --help'.\n")
set(exit 2)
check(check_output_missing)
if(EXISTS "${OUT}/refused.c")
    message(SEND_ERROR "check_steps_refused: refused.c was written")
endif()

# Given twice, even alike.
set(args --check coast.ink coast --steps 10 --steps 10 -o twice.c)
set(stderr "inkamath: --steps is given twice\nTry 'inkamath --help'.\n")
set(exit 2)
check(check_steps_twice)
# And so are -o and --float, a repetition adding nothing (C297).
set(args --check coast.ink coast -o twice.c -o twice.c)
set(stderr "inkamath: -o is given twice\nTry 'inkamath --help'.\n")
set(exit 2)
check(check_output_twice)
set(args --check coast.ink coast --float --float -o twice.c)
set(stderr "inkamath: --float is given twice\nTry 'inkamath --help'.\n")
set(exit 2)
check(check_float_twice)
# And the rest, each one alike (C302).
foreach(twice IN ITEMS --compile --check --echo -i)
    set(args ${twice} ${twice} coast.ink)
    set(stderr "inkamath: ${twice} is given twice\nTry 'inkamath --help'.\n")
    set(exit 2)
    check(twice_${twice})
endforeach()

# Only a check that writes a program has steps: not a header, a transcript
# replayed, or files run.
file(WRITE "${OUT}/said.ink" ">> 1+1\n2\n")
foreach(case IN ITEMS "compile;--compile;coast.ink;fall;-o;fall.h"
                      "transcript;--check;said.ink"
                      "run;coast.ink")
    list(POP_FRONT case name)
    set(args ${case} --steps 10)
    set(stderr "inkamath: --steps takes --check with -o check.c\nTry 'inkamath --help'.\n")
    set(exit 2)
    check(check_steps_${name}_refused)
endforeach()
# An instance with no -o is told what --check takes, as without --steps.
set(args --check coast.ink coast --steps 10)
set(stderr "inkamath: --check takes a transcript, or a file, optionally an instance or a model it defines, and -o check.c\nTry 'inkamath --help'.\n")
set(exit 2)
check(check_steps_usage)
# And so is one a word too many, with --steps or --float, where each said it
# takes --check with an instance (C298).
foreach(option IN ITEMS "steps;--steps;5" "float;--float")
    list(POP_FRONT option name)
    set(args --check coast.ink coast extra ${option} -o extra.c)
    set(stderr "inkamath: --check takes a transcript, or a file, optionally an instance or a model it defines, and -o check.c\nTry 'inkamath --help'.\n")
    set(exit 2)
    check(check_${name}_extra_usage)
endforeach()

# The step takes a single value for an input whose model states no size, so
# an instance giving a matrix is refused by name, where it was read past its
# one cell, and so is a model reading a cell past it; each says how to state
# the size (C83).
file(WRITE "${OUT}/wide.ink" "mm(x_n) = {\n    y_n = [1 2]*x_n\n}\nv = mm(x_n = [n; 1])\n")
set(args --check wide.ink v -o wide.c)
set(stderr "inkamath: v.x_(0) has 2 cells, where the compiled step takes a single value, as mm states no size for x: write 'x_n[j<=2]'\n")
set(exit 1)
check(check_matrix_input)
# A tensor too, by its slices (C130).
file(WRITE "${OUT}/deep.ink" "mt(X_n) = {\n    t_n = 2*X_n\n}\nv = mt(X_n = [n; 1;; 2; 3])\n")
set(args --check deep.ink v -o deep.c)
set(stderr "inkamath: v.X_(0) has 4 cells, where the compiled step takes a single value, as mt states no size for X: write 'X_n[b<=2, j<=2, k<=1]'\n")
set(exit 1)
check(check_tensor_input)
# C138: a function whose clauses give different shapes is refused, as a
# sequence's are, where the step stretched them to the largest. It was the one
# way known to reach C137's guard in --check, which nothing reaches now.
file(WRITE "${OUT}/c138.ink" "h(z) = 3\nh(z) | z > 2 = [1; 2]\ny_n = h(x_n)\n")
set(args --compile c138.ink -o c138.h)
set(stderr "inkamath: cannot compile y: the clauses of h have different shapes\n")
set(exit 1)
check(compile_c138)
file(WRITE "${OUT}/celled.ink" "cl(x_n) = {\n    y_n = x_n[2]\n}\ncm(u_m) = {\n    z_m = u_(m-1)[2, 3]\n}\n"
     "ct(X_n) = {\n    t_n = X_n[1, 2, 1]\n}\n")
set(args --compile celled.ink cl -o cl.h)
set(stderr "inkamath: cannot compile y: x is a single value, as cl states no size for it: write 'x_n[j<=2]'\n")
set(exit 1)
check(compile_unstated_cell)
set(args --compile celled.ink cm -o cm.h)
set(stderr "inkamath: cannot compile z: u is a single value, as cm states no size for it: write 'u_m[j<=2, k<=3]'\n")
set(exit 1)
check(compile_unstated_matrix)
set(args --compile celled.ink ct -o ct.h)
set(stderr "inkamath: cannot compile t: X is a single value, as ct states no size for it: write 'X_n[b<=1, j<=2, k<=1]'\n")
set(exit 1)
check(compile_unstated_tensor)
# Read through a sequence that is a single value because the input is (C144).
file(WRITE "${OUT}/through.ink" "mon(x_n) = {\n    e_n = 2*x_n\n    k_n = e_n[2]\n}\n")
set(args --compile through.ink mon -o mon.h)
set(stderr "inkamath: cannot compile k: e reads x, a single value, as mon states no size for it: write 'x_n[j<=2]'\n")
set(exit 1)
check(compile_unstated_through)
# So is a parameter with no default, read past its cell.
file(WRITE "${OUT}/nodefault.ink" "m(A, x_n) = {\n    y_n = A[1, 2]*x_n\n}\n")
set(args --compile nodefault.ink m -o m.h)
set(stderr "inkamath: cannot compile y: A is a single value, as m gives it no default: give it one of its size, at least j<=1, k<=2\n")
set(exit 1)
check(compile_unstated_parameter)
# And through a function's parameter, as iekf.ink's blk(X_n) reads X0.
file(APPEND "${OUT}/nodefault.ink" "blk(X) = X[1, 2]\ntwice(Y) = 2*blk(Y)\n"
     "mb(A, x_n) = {\n    X_n = A\n    y_n = twice(X_n)*x_n\n}\ncl(x_n) = {\n    y_n = blk(x_n)\n}\n")
set(args --compile nodefault.ink mb -o mb.h)
set(stderr "inkamath: cannot compile y: X reads A, a single value, as mb gives it no default: give it one of its size, at least j<=1, k<=2\n")
set(exit 1)
check(compile_unstated_argument)
set(args --compile nodefault.ink cl -o cl.h)
set(stderr "inkamath: cannot compile y: x is a single value, as cl states no size for it: write 'x_n[j<=1, k<=2]'\n")
set(exit 1)
check(compile_unstated_argument_input)

# Every run of --check walks the prelude's definitions, so that its programs
# hold the compiled functions to the walk (DESIGN.md): log, 16 references
# deep where called compiled it is one, takes an input past 256.
file(WRITE "${OUT}/walked.ink" "dv(k) = dv(k - 1)\ndv(k) | k < 1 = log(~3)\nmm(x_n) = {\n    y_n = x_n\n}\nv = mm(x_n = dv(245))\n")
set(args --check walked.ink v -o walked.c)
set(stderr "inkamath: v.x_(0): evaluation nests more than 256 references deep\n")
set(exit 1)
check(check_walked)

# Nor does a term the file asked before: its main run forgets them (C106).
file(APPEND "${OUT}/walked.ink" "v.x_(0)\n")
set(args --check walked.ink v -o walked.c)
set(stderr "inkamath: v.x_(0): evaluation nests more than 256 references deep\n")
set(exit 1)
check(check_walked_asked)

# C140: a call's value given to a function is computed once, where written at
# each reading of the parameter it tripled at each call nested here, 1.2 MB.
file(WRITE "${OUT}/c140.ink" "f(p) = p/2 + p/3 + p/5\nx_0 = 1\nx_n = f(f(f(f(f(f(f(f(f(f(x_(n-1)))))))))))\n")
set(args --compile c140.ink -o c140.h)
check(compile_c140)
holds(compile_c140 c140.h "    const double t8_ = t7_ / 2.0 + t7_ / 3.0 + t7_ / 5.0;\n"
      "    m_->x[0] = m_->index_ == 0 ? 1.0 : t8_ / 2.0 + t8_ / 3.0 + t8_ / 5.0;\n")
file(SIZE "${OUT}/c140.h" size)
if(size GREATER 4096)
    message(SEND_ERROR "compile_c140: c140.h is ${size} bytes")
endif()

# C230: a cell of a call read in a sum or in a cell compiled every cell of
# the call at each reading, 300^3 cells here and 4^10 down g's chain.
file(WRITE "${OUT}/c230.ink" "f(x)[i<=300] = x*i\nh(x)[i<=300] = f(x)[i] + 1\ng0(x)[i<=4] = x*i\n")
foreach(k RANGE 1 10)
    math(EXPR j "${k} - 1")
    file(APPEND "${OUT}/c230.ink" "g${k}(x)[i<=4] = g${j}(x)[i] + 1\n")
endforeach()
file(APPEND "${OUT}/c230.ink" "y_n = sum_(i=1)^300 h(n)[i] + sum_(i=1)^4 g10(n)[i]\n")
set(args --compile c230.ink -o c230.h)
check(compile_c230)
holds(compile_c230 c230.h "(double)m_->index_ * 300.0 + 1.0) + ((double)m_->index_ * 1.0 + 1.0 + 1.0 + 1.0 + 1.0 + 1.0 + 1.0 + 1.0 + 1.0 + 1.0 + 1.0 + ((double)m_->index_ * 2.0 + 1.0")

# C357: a call read again was written into each reader's text, so Cholesky's
# recurrence by cells took 47 s at 14x14 and ran out of memory at 16x16.
file(WRITE "${OUT}/c357.ink" [=[
mc(A[j<=n, k<=n])[i<=n, k<=n] = mr(A, i, k)
mr(A, i, k) | i < k = 0
mr(A, i, k) | i == k and mp(A, k) > 0 = mp(A, k)^(1/2)
mr(A, i, k) | i > k = (A[i,k] - sum_(q=1)^(k-1) mr(A, i, q)*mr(A, k, q))/mr(A, k, k)
mp(A, k) = A[k,k] - sum_(q=1)^(k-1) mr(A, k, q)^2
big(A_n[j<=16, k<=16]) = {
    e_n = mc(A_n)
}
]=])
set(args --compile c357.ink big -o c357.h)
check(compile_c357)

# C231: under grad, where a sum's part is there was written out at each
# partial sum, each term's being where any cell of m's is: 999^2 times 22 KB.
file(WRITE "${OUT}/c231.ink" "A[i<=32] = mod(3*i, 7) - 3 + 1/2\nrelu(z)[i] | z[i] > 0 = z[i]\nrelu(z)[i] = 0\nh(m) = sum_(i=1)^999 m[1 + mod(i, 32)]\nw_0 = 1/2\nw_n = w_(n-1) - grad_(v = w_(n-1)) h(relu(relu(A*v)))/1024\n")
set(args --compile c231.ink -o c231.h)
check(compile_c231)

# C155: as is a call of the prelude's, where it was written at each reading.
file(WRITE "${OUT}/c155.ink" "h(t) = (t - 1)/t\nh(t) | t == 1 = t\nh(t) | t == 0 = 1\nf(z) = h(exp(z))\nx_0 = 1/2\nx_n = f(f(x_(n-1)))\n")
set(args --compile c155.ink -o c155.h)
check(compile_c155)
file(READ "${OUT}/c155.h" text)
string(REGEX MATCHALL "c155_exp\\(" calls "${text}")
list(LENGTH calls calls)
if(NOT calls EQUAL 3)
    message(SEND_ERROR "compile_c155: c155_exp written ${calls} times, not 3")
endif()

# Under grad, its value and part are temporaries where its argument moves:
# exp's in attention, its value once for each score and in reverse mode its
# slope once for each score an adjoint reaches, 14 of the 16: batch 2's
# second token in head 1 has none, G's 1 and 1 by WO's 1 and -1 an exact 0
# (compile/tensorgrad.ink).
set(args --compile "${CMAKE_CURRENT_LIST_DIR}/compile/tensorgrad.ink" attn -o attn.h)
check(compile_attend_calls)
file(READ "${OUT}/attn.h" text)
string(REGEX MATCHALL "attn_exp_jx\\(" values "${text}")
string(REGEX MATCHALL "attn_exp_dx\\(" parts "${text}")
list(LENGTH values values)
list(LENGTH parts parts)
file(SIZE "${OUT}/attn.h" size)
if(NOT values EQUAL 17 OR NOT parts EQUAL 15 OR NOT size LESS 40000)
    message(SEND_ERROR "compile_attend_calls: attn_exp_jx( ${values} times, attn_exp_dx( "
                       "${parts}, in ${size} bytes")
endif()

# The compiler's adjoint step (DESIGN.md, compile/adjoint.ink): the body
# written once and swept back, a function of the prelude's slope once for
# each call, and the header under 100,000 bytes.
function(written name file)
    file(READ "${OUT}/${file}" text)
    file(SIZE "${OUT}/${file}" size)
    if(NOT size LESS 100000)
        message(SEND_ERROR "${name}: ${file} is ${size} bytes")
    endif()
    math(EXPR last "${ARGC} - 1")
    foreach(i RANGE 2 ${last} 2)
        math(EXPR n "${i} + 1")
        string(REGEX MATCHALL "${ARGV${i}}" found "${text}")
        list(LENGTH found found)
        if(NOT found EQUAL ${ARGV${n}})
            message(SEND_ERROR "${name}: ${ARGV${i}} written ${found} times, not ${ARGV${n}}")
        endif()
    endforeach()
endfunction()
set(adjoint "${CMAKE_CURRENT_LIST_DIR}/compile/adjoint.ink")
set(args --compile ${adjoint} layer -o layer.h)
check(compile_adjoint_fit)
holds(compile_adjoint_fit layer.h [[    const double t19_ = t17_ + (0.0 - m_->y[0][3][0]);
    const double t20_ = 2.0 * pow(t19_, 1.0);
    const double t21_ = 0.5 * t20_;
    const double t22_ = t21_ * t18_;
]] [[    m_->W[0][3][3] = m_->index_ == 0 ? 0.5 : m_->W[1][3][3] + (0.0 - m_->eta * (isnan(t29_) ? NAN : t22_ * m_->x[0][3][0]));
]])
written(compile_adjoint_fit layer.h "layer_tanh_dx\\(" 5)
set(args --compile ${adjoint} wide -o wide.h)
check(compile_adjoint_wide)
written(compile_adjoint_wide wide.h "wide_tanh_dx\\(" 17)
set(args --compile ${adjoint} norm -o norm.h)
check(compile_adjoint_norm)
written(compile_adjoint_norm norm.h "norm_exp_dx\\(" 13 "norm_log_dx\\(" 4)
# A matrix read by its cells is tested for NaN once (DESIGN.md).
file(READ "${OUT}/norm.h" text)
string(REGEX MATCHALL "isnan\\(" tests "${text}")
list(LENGTH tests tests)
file(SIZE "${OUT}/norm.h" size)
if(NOT tests LESS 150 OR NOT size LESS 40000)
    message(SEND_ERROR "compile_nan_once: isnan( ${tests} times in ${size} bytes")
endif()
# The reverse sweep's conditions written once (DESIGN.md): each condition of
# 'hidden' once, a flag the cells read, where each cell of U wrote all three
# units'.
set(args --compile ${adjoint} hidden -o hidden.h)
check(compile_sweep_conditions)
file(READ "${OUT}/hidden.h" text)
string(REGEX MATCHALL " != 0\\)" conditions "${text}")
list(LENGTH conditions conditions)
if(NOT conditions LESS 20)
    message(SEND_ERROR "compile_sweep_conditions: ' != 0)' written ${conditions} times")
endif()
# Nested, not flattened, which writes 15 as well: a flag of conditions holds
# one '||'.
string(REGEX MATCHALL "const int t[0-9]+_ = [^;\n]*" flags "${text}")
foreach(flag IN LISTS flags)
    string(REGEX MATCHALL " \\|\\| " ors "${flag}")
    list(LENGTH ors ors)
    if(flag MATCHES " != 0" AND NOT ors EQUAL 1)
        message(SEND_ERROR "compile_sweep_conditions: ${flag}")
    endif()
endforeach()
# Shared at every value written twice (DESIGN.md): Rodrigues' formula of the
# rigid-body exploration takes its norm once, where each cell wrote it three
# times, and C133's guard reads the value of s that a temporary holds by its
# name, where it wrote it again thrice; a turn by n reads its cosine, a
# product's operand, by the operand's name, where its guard wrote it again.
file(WRITE "${OUT}/rod.ink"
     "hat(w) = [0, -w[3], w[2]; w[3], 0, -w[1]; -w[2], w[1], 0]\nI3[j<=3, k<=3] = j == k\n"
     "nrm(v) = (v'*v)^(1/2)\nrod(w) | w'*w == 0 = I3\n"
     "rod(w) = I3 + sin(nrm(w))/nrm(w)*hat(w) + (1 - cos(nrm(w)))/(w'*w)*hat(w)*hat(w)\n"
     "M_0 = [1/20; 2; 0]\nM_n = rod(-M_(n-1)/10)*M_(n-1)\n")
set(args --compile rod.ink -o rod.h)
check(compile_values_once)
file(WRITE "${OUT}/c133s.ink" "s(z) = (z > 2)*z\nk(z) = 0\nk(z) | s(z) > -1 = z\nw_n = grad_(t = x_n) k(t)\n")
set(args --compile c133s.ink -o c133s.h)
check(compile_values_once)
file(WRITE "${OUT}/turn.ink" "R(t) = [cos(t), -sin(t); sin(t), cos(t)]\nx_0 = [1, 0; 0, 1]\n"
     "x_n | cos(n/10) > 0 = R(n/10)*x_(n-1)\nx_n = x_(n-1)\n")
set(args --compile turn.ink -o turn.h)
check(compile_values_once)
file(READ "${OUT}/turn.h" turn)
string(REGEX MATCHALL "cos\\(\\(double\\)m_->index_ / 10\\.0\\)" cosines "${turn}")
list(LENGTH cosines cosines)
string(REGEX MATCHALL "const double t[0-9]+_ = t[0-9]+_;" copies "${turn}")
list(LENGTH copies copies)
file(READ "${OUT}/rod.h" rod)
string(LENGTH "${rod}" bytes)
string(REGEX MATCHALL "sqrt\\(" norms "${rod}")
list(LENGTH norms norms)
file(READ "${OUT}/c133s.h" text)
string(REGEX MATCHALL "m_->x\\[0\\] > 2\\.0" values "${text}")
list(LENGTH values values)
if(NOT norms EQUAL 1 OR NOT bytes LESS 8000 OR NOT values EQUAL 1 OR NOT cosines EQUAL 1
   OR NOT copies EQUAL 0)
    message(SEND_ERROR "compile_values_once: 'sqrt(' ${norms} times in ${bytes} bytes, "
                       "s's value ${values} times, the cosine ${cosines} times and "
                       "${copies} temporaries copying another")
endif()
# A compiled step split into functions (DESIGN.md): compile/transformer.ink's
# forward pass, 'inf', and U trained beneath it, 'tune', steps of 272,495 and
# 451,298 bytes whole, each written as functions of under 32,768 bytes, the
# init's weights among them.
foreach(model inf tune)
    set(args --compile "${CMAKE_CURRENT_LIST_DIR}/compile/transformer.ink" ${model} -o ${model}.h)
    check(compile_step_split_${model})
    file(READ "${OUT}/${model}.h" rest)
    set(largest 0)
    set(stepped OFF)
    # Every function, whatever precedes its name: a line at the margin ending
    # in ') {'. The step among them, or nothing was measured.
    set(opens "\n[A-Za-z][^\n]*\\) {\n")
    string(REGEX MATCH "${opens}" opening "${rest}")
    while(opening)
        string(FIND "${opening}" " ${model}_step(" named)
        if(NOT named EQUAL -1)
            set(stepped ON)
        endif()
        string(FIND "${rest}" "${opening}" at)
        string(SUBSTRING "${rest}" ${at} -1 rest)
        string(FIND "${rest}" "\n}\n" size)
        if(size GREATER largest)
            set(largest ${size})
        endif()
        string(SUBSTRING "${rest}" ${size} -1 rest)
        string(REGEX MATCH "${opens}" opening "${rest}")
    endwhile()
    if(NOT stepped)
        message(SEND_ERROR "compile_step_split: ${model}.h has no ${model}_step")
    elseif(NOT largest LESS 32768)
        message(SEND_ERROR "compile_step_split: ${model}.h has a function of ${largest} bytes")
    endif()
endforeach()
# An 'any' of equal operands is the operand, a NaN test of equal cells one.
file(WRITE "${OUT}/twice.ink" "m(x_n) = {\n    up(z)[i] | z[i] > 0 = z[i]\n    up(z)[i] = 0\n"
     "    d_n = grad_(t = x_n) t*(up([t; t])[2] > 0)\n}\n")
set(args --compile twice.ink m -o twice.h)
check(compile_any_twice)
file(READ "${OUT}/twice.h" text)
string(REGEX MATCHALL "const int t[0-9]+_ = [^;\n]*" flags "${text}")
foreach(flag IN LISTS flags)
    if(flag MATCHES "= (.*) \\|\\| (.*)$" AND CMAKE_MATCH_1 STREQUAL CMAKE_MATCH_2)
        message(SEND_ERROR "compile_any_twice: ${flag}")
    endif()
endforeach()
# A gradient by several names compiled (DESIGN.md, test/compile/gradmany.ink):
# a struct of its parts, the body written once and swept back once, tanh's
# slope once for each unit, where four grads of one name write it 7 times in
# 15,798 bytes.
set(args --compile "${CMAKE_CURRENT_LIST_DIR}/compile/gradmany.ink" net -o net.h)
check(compile_gradmany_net)
holds(compile_gradmany_net net.h [[    struct {
        double W[1][3][2];
        double V[1][1][3];
        double c[1][3][1];
        double d[1];
    } g;
]])
written(compile_gradmany_net net.h "net_tanh_dx\\(" 4)
file(SIZE "${OUT}/net.h" size)
if(NOT size LESS 15798)
    message(SEND_ERROR "compile_gradmany_net: net.h is ${size} bytes")
endif()
# One whose points read its own parts is refused once, by its name, as any
# sequence that reads itself, and so is what reads it.
file(WRITE "${OUT}/itself.ink" "L(a, b) = (a*b - 1)^2\n"
     "s_n = grad_(a = s.b_(n-1), b = 2) L(a, b)\ny_n = s.a_n\n")
set(args --compile itself.ink)
set(stdout "cannot compile s: its shape depends on itself
cannot compile y: s, which cannot be compiled
")
set(exit 1)
check(compile_gradmany_itself)
# One read whole is refused in the interpreter's words, its index as written.
file(WRITE "${OUT}/whole.ink" "L(a, b) = (a*b - 1)^2\ng_n = grad_(a = n, b = 2) L(a, b)\n"
     "p = grad_(a = 1, b = 2) L(a, b)\nq(x) = grad_(a = x, b = 2) L(a, b)\n"
     "r(x)_k = grad_(a = x + k, b = 2) L(a, b)\n"
     "u_n = g_(n-1)\nv_n = p + n\nw_n = p_n\nx_n = q(n)\ny_n = r(n)_2\nz_n = g + n\n")
set(args --compile whole.ink)
set(stdout "cannot compile u: g is a gradient by several names; read one of its parts (g.a_(...))
cannot compile v: p is a gradient by several names; read one of its parts (p.a)
cannot compile w: p is not a sequence
cannot compile x: q is a gradient by several names; read one of its parts (q(...).a)
cannot compile y: r is a gradient by several names; read one of its parts (r(...).a_(...))
cannot compile z: g is a gradient by several names; read one of its parts (g.a)
")
set(exit 1)
check(compile_gradmany_whole)
# A part of one that takes arguments, which the interpreter reads, is refused
# for now, and one that takes none is read with none.
file(WRITE "${OUT}/taking.ink" "L(a, b) = (a*b - 1)^2\ng_n = grad_(a = n, b = 2) L(a, b)\n"
     "r(x)_k = grad_(a = x + k, b = 2) L(a, b)\nw_n = r.a_n\ny_n = r(n).a_2\n"
     "z_n = g(1).a_n\n")
set(args --compile taking.ink)
set(stdout "cannot compile w: r, a gradient by several names that takes arguments, for now
cannot compile y: r, a gradient by several names that takes arguments, for now
cannot compile z: g takes no arguments
")
set(exit 1)
check(compile_gradmany_taking)
# C328: a sequence compiled on the way by a grad keeps the names it took,
# z's t4_, which the grad does not take again.
file(WRITE "${OUT}/c328.ink" [[m(x_n[i<=2]) = {
    r(t) | t < 0 = 0
    r(t) = t
    W_0 = [1 -1; 1/2 1]
    W_n = W_(n-1) - grad_(V = W_(n-1)) (exp((V*x_n)[1]) + r((V*z_n)[2]))/4
    z_n = x_n*exp(x_n[1])
}
]])
set(args --compile c328.ink m -o c328.h)
check(compile_c328)
written(compile_c328 c328.h "double t4_ " 1)

# C315: a temporary that may be NaN was known by its name, which a limit's
# function names its own by: the step's t1_, atan's, and nw's v0_, exp's,
# made nw's t1_ and q's v1_ tested for a NaN nothing gives them.
file(WRITE "${OUT}/c315.ink" "F(M) = 1\nG(v) = v*v\nr(s, v) = exp(s) - v\nnw(v)_0 = 0\nnw(v)_k = nw(v)_(k-1) - r(nw(v)_(k-1), v)/grad_(s = nw(v)_(k-1)) r(s, v)\nq(v)_0 = v\nq(v)_k = F(G(G(q(v)_(k-1)) + 1))*q(v)_(k-1)/2\na_n = G(G(n) + 1) + grad_(s = n) atan(s)\nb_n = lim nw(n + 1)\nc_n = lim q(n)\n")
set(args --compile c315.ink -o c315.h)
check(compile_c315)
holds(compile_c315 c315.h "        const double v2_ = v0_ + (0.0 - arg_v);\n"
      "        const double t_ = 1.0 * t1_ / 2.0;\n")

# C317: a call's memo key wrote out where its argument's part is there, a
# graph each f shares five times: 287 s and 4 GB before, 0.02 s since.
file(WRITE "${OUT}/c317.ink" "relu(z)[i,j] | z[i,j] > 0 = z[i,j]\nrelu(z)[i,j] = 0\nf(z)[i,j] = z[i,j] - sum_(k=1)^4 z[i,k]/4\nm(x_n[i<=1, j<=4]) = {\n    c_0 = [0 0 0 0]\n    c_n = c_(n-1) - grad_(r = c_(n-1)) sum_(j=1)^4 f(f(f(f(f(f(relu(x_n + r)))))))[1,j]^2/1000\n}\n")
set(args --compile c317.ink m -o c317.h)
check(compile_c317)

# C319: an input read bare is refused as the interpreter refuses it, where a
# function given it refused a place in a 1x1 matrix.
file(WRITE "${OUT}/c319.ink" "m(x_n[i<=2, j<=2]) = {\n    f(z)[i,j] = z[i,j]\n    y_n = f(x)[2,1]\n}\n")
set(args --compile c319.ink m -o c319.h)
set(stderr "inkamath: cannot compile y: x is a sequence; index it\n")
set(exit 1)
check(compile_c319)

# C153: a power of 1/2 is C's sqrt, rounded correctly, where pow need not be.
file(WRITE "${OUT}/c153.ink" "x_0 = 2\nx_n = x_(n-1)^(1/2) + x_(n-1)^0.5\n")
set(args --compile c153.ink -o c153.h)
check(compile_c153)
holds(compile_c153 c153.h "2.0 : sqrt(0.0 + m_->x[1]) + sqrt(0.0 + m_->x[1]);\n")
set(args --compile c153.ink --float -o c153f.h)
check(compile_float_c153)
holds(compile_float_c153 c153f.h "2.0f : sqrtf(0.0f + m_->x[1]) + sqrtf(0.0f + m_->x[1]);\n")

# A root of 0 to a negative power divides by an exact zero, as 0^-1 does.
file(WRITE "${OUT}/pole.ink" "y_n = 0^(-1/2)\n")
set(args --compile pole.ink -o pole.h)
set(stderr "inkamath: cannot compile y: division by zero\n")
set(exit 1)
check(compile_pole)

# C286: grad's u^(c-1) at a constant 0 is C's pow's inf, folded.
file(WRITE "${OUT}/c286.ink" "y_n = grad_(t = 0) t^(1/2)\n")
set(args --compile c286.ink -o c286.h)
check(compile_c286)
holds(compile_c286 c286.h "(isnan(t0_) ? NAN : INFINITY);\n")

# C395: a constant folded from a number approximated past a thousand digits,
# which the interpreter marks, is named in the header: 2^4000 > 2^3999 is 0
# there, and 3^3000/3^2999 NaN.
file(WRITE "${OUT}/c395.ink" "y_n = n + (2^4000 > 2^3999)\nz_n = n*(3^3000/3^2999)\nw_n = n\n")
set(args --compile c395.ink -o c395.h)
check(compile_c395)
holds(compile_c395 c395.h [[ * sequence: w, y and z. A value folded for y and z from a number approximated
 * past a thousand digits, as the interpreter marks it, may be far from exact.
]])

# C396: with --float, a constant folded in double from inexact numbers, a
# machine epsilon by halving, is named in the header: 2^-52, where a float's
# halving gives 2^-23. A number made inexact, ~(17/4), is not.
file(WRITE "${OUT}/c396.ink" "ep(x) | 1 + x/2 > 1 = ep(x/2)\nep(x) = x\n"
     "y_n = n*ep(~1)\nz_n = n*~(17/4)\n")
set(args --compile c396.ink --float -o c396.h)
check(compile_float_c396)
holds(compile_float_c396 c396.h [[ * sequence: y and z. Every value is a float, and every operation rounds to
 * one. A value folded for y from inexact numbers was computed at the
 * interpreter's precision, not a float's, and a float step may compute
 * another.
]])

# C161: a float's inverse starts from the identity in floats, which MSVC's
# /W4 asks of an int converted to one.
file(WRITE "${OUT}/c161.ink" "z_n = [2 1; 1 n]^-1*[1; 1]\n")
set(args --compile c161.ink --float -o c161.h)
check(compile_float_c161)
holds(compile_float_c161 c161.h "; ++j_) r_[i_][j_] = i_ == j_ ? 1.0f : 0.0f;\n")

# C90: the clause --check keeps is 0 where a guard of 'and' reads NaN, as no
# clause is taken, where the NaN was converted to an int.
file(WRITE "${OUT}/c90.ink" "gate(x_n) = {\n    y_0 = 1\n    y_n = y_(n-1) + x_n\n    z_n | x_n > 0 and y_(n-2) > 0 = 1\n    z_n = 0\n}\ng = gate(x_n = 1)\n")
set(args --check c90.ink g -o c90.c)
check(check_c90)
holds(check_c90 c90.c [[m_->z_clause_ = isnan(t1_) ? 0 : t1_ != 0.0 ? 1 : 2;]])

# A NaN reaches every term that reads it (DESIGN.md): the steps that
# test/compile/nan.ink specifies, each compiled with nothing to say.
set(nan "${CMAKE_CURRENT_LIST_DIR}/compile/nan.ink")
foreach(model IN ITEMS level pick refuse ramp both walked tuned either)
    set(args --compile ${nan} ${model} -o ${model}.h)
    check(compile_nan_${model})
endforeach()
holds(compile_nan_level level.h
      [[    m_->y[0] = level_lim0(m_, 0, m_->x[0]);
    m_->high[0] = isnan(m_->y[0]) ? NAN : m_->y[0] > 0.5 ? 1.0 : 0.0;
]]
      [[ * name_(n-k) for each sequence: x, y and high. A term the interpreter would
 * refuse is NaN, and so is every term that reads one, through a guard or a
 * comparison as through arithmetic. Built with -ffinite-math-only, which
 * -ffast-math implies, GCC removes the tests that make it so, and Clang warns
 * of each NaN.
]])
holds(compile_nan_pick pick.h
      [[    m_->y[0] = isnan(m_->x[0]) ? NAN : m_->x[0] > 0.0 ? m_->x[0] : NAN;
    m_->on[0] = isnan(m_->y[0]) ? NAN : m_->y[0] > 2.0 ? 1.0 : 0.0;
    m_->p[0] = (isnan(m_->y[0]) ? NAN : pow(1.0, m_->y[0]));
]])
holds(compile_nan_refuse refuse.h
      [[    m_->y[0] = isnan(m_->x[0]) ? NAN : m_->x[0] <= 0.0 ? NAN : m_->x[0];
    m_->z[0] = isnan(m_->y[0]) ? NAN : m_->y[0] > 1.0 ? 1.0 : 0.0;
]])
holds(compile_nan_ramp ramp.h
      [[    m_->y[0][1][0] = isnan(m_->x[0] - 2.0) ? NAN : m_->x[0] - 2.0 > 0.0 ? m_->x[0] - 2.0 : NAN;
    if (isnan(m_->y[0][0][0]) || isnan(m_->y[0][1][0]))
        for (int i_ = 0; i_ < 2; ++i_)
            for (int j_ = 0; j_ < 1; ++j_) m_->y[0][i_][j_] = NAN;
    m_->g[0][0][0] = isnan(m_->y[0][0][0]) ? NAN : m_->y[0][0][0] > 1.0 ? 1.0 : 0.0;
    m_->g[0][1][0] = isnan(m_->y[0][1][0]) ? NAN : m_->y[0][1][0] > 1.0 ? 1.0 : 0.0;
    if (isnan(m_->g[0][0][0]) || isnan(m_->g[0][1][0]))
        for (int i_ = 0; i_ < 2; ++i_)
            for (int j_ = 0; j_ < 1; ++j_) m_->g[0][i_][j_] = NAN;
    m_->h[0] = isnan(m_->y[0][0][0]) ? NAN : m_->y[0][0][0] > 0.5 ? 1.0 : 0.0;
]])
holds(compile_nan_both both.h
      [[    const double t0_ = (isnan(m_->x[0]) ? NAN : m_->x[0] < 3.0 ? 1.0 : 0.0);
    const double t1_ = (t0_ == 0.0 ? 0.0 : t0_ != t0_ ? NAN : (m_->index_ < 1 ? NAN : (isnan(m_->y[1]) ? NAN : m_->y[1] > 0.5 ? 1.0 : 0.0)));
    m_->w[0] = isnan(t1_) ? NAN : t1_ != 0.0 ? 1.0 : 0.0;
    m_->y[0] = m_->index_ == 0 ? 2.0 : isnan(m_->x[0]) ? NAN : m_->x[0] > 0.0 ? m_->x[0] : NAN;
    m_->z[0] = isnan(m_->y[0]) ? NAN : m_->y[0] != 0.0 ? 1.0 : 0.0;
]])
holds(compile_nan_walked walked.h
      [[        const double t_ = isnan(arg_a) ? NAN : arg_a > 1.0 ? 0.0 : t1_ / 2.0;
]]
      [[    m_->y[0] = walked_lim0(m_, 0, isnan(m_->x[0]) ? NAN : m_->x[0] > 0.0 ? m_->x[0] : NAN);
]])
holds(compile_nan_tuned tuned.h
      [[    m_->a = isnan(m_->g) ? NAN : m_->g > 0.0 ? m_->g : NAN;
    m_->k = isnan(m_->a) ? NAN : m_->a > 1.0 ? 1.0 : 0.0;
]])
holds(compile_nan_either either.h
      [[    m_->y[0] = isnan(m_->x[0]) ? NAN : m_->x[0] > 0.0 ? m_->x[0] : NAN;
    const double t0_ = (isnan(m_->x[0]) ? NAN : m_->x[0] > 3.0 ? 1.0 : 0.0);
    const double t1_ = (t0_ == 0.0 ? ((isnan(m_->y[0]) ? NAN : m_->y[0] > 0.5 ? 1.0 : 0.0)) : t0_ != t0_ ? NAN : 1.0);
    m_->o[0] = isnan(t1_) ? NAN : t1_ != 0.0 ? 1.0 : 0.0;
]])
# A guard that folds to NaN is refused, as the interpreter refuses it, in the
# name of its definition and in the words of the operator that reads it.
file(WRITE "${OUT}/nanguard.ink" "h_n = x_n\nh_n | 0/~0 = 1\nk_n = x_n and 0/~0\n")
set(args --compile nanguard.ink)
set(stdout "cannot compile h: a guard needs a number, not -nan\ncannot compile k: and needs a number, not -nan\n")
set(exit 1)
check(compile_nan_guard_refused)

# A cell of a constant matrix with a NaN cell folds, which leaves the header
# one that writes no NaN.
file(WRITE "${OUT}/folded.ink" "k(x_n) = {\n    y_n = ([0/~0; 1])[2] + x_n\n}\n")
set(args --compile folded.ink k -o folded.h)
check(compile_nan_folded)
holds(compile_nan_folded folded.h [[ * name_(n-k) for each sequence: x and y.
 */
]])

# NAN in a name is no NaN written: pid_clamped as NAN_clamped is its header,
# renamed.
set(args --compile "${CMAKE_CURRENT_LIST_DIR}/compile/pid_clamped.ink" -o NAN_clamped.h)
check(compile_nan_named)
file(READ "${OUT}/NAN_clamped.h" named)
file(READ "${CMAKE_CURRENT_LIST_DIR}/compile/expected/pid_clamped.h" expected)
string(REPLACE "NAN_CLAMPED_" "PID_CLAMPED_" named "${named}")
string(REPLACE "NAN_clamped" "pid_clamped" named "${named}")
if(NOT named STREQUAL expected)
    message(SEND_ERROR "compile_nan_named: NAN_clamped.h is not pid_clamped.h renamed")
endif()

# The clause --check keeps is 0 where the guard reads NaN.
foreach(instance IN ITEMS pair any)
    set(args --check ${nan} ${instance} -o ${instance}.c)
    check(check_nan_${instance})
endforeach()
holds(check_nan_pair pair.c [[m_->w_clause_ = isnan(t1_) ? 0 : t1_ != 0.0 ? 1 : 2;]])
holds(check_nan_any any.c [[m_->o_clause_ = isnan(t1_) ? 0 : t1_ != 0.0 ? 1 : 2;]])

# An input whose model states its size, x_n[j<=2], is a pointer to its cells,
# row by row, as test/compile/inputs.ink specifies (C83).
foreach(model IN ITEMS dot ctl turn avg)
    set(args --compile ${inputs} ${model} -o ${model}.h)
    check(compile_inputs_${model})
endforeach()
holds(compile_inputs_dot dot.h [[/* Using it:
 *
 *     dot m;
 *     dot_init(&m);
 *     dot_step(&m, x);  once for each index, the first 0
 *     m.y[0]  is then y_n
 *
 * A step takes x_n (2x1), the input at its index. An input of more than one
 * cell is a pointer to its cells, row by row. After a step, m.name[k] is
 * name_(n-k) for each sequence: x and y.
 */
]] [[typedef struct dot {
    long long index_;
    double x[1][2][1];
    double y[1];
} dot;
]] [[DOT_API_ void dot_step(dot* m_, const double x_in_[2]) {
    ++m_->index_;
    memcpy(m_->x[0], x_in_, sizeof m_->x[0]);
]])
holds(compile_inputs_ctl ctl.h [[
 * A step takes r_n and s_n (2x1), the inputs at its index. An input of more
 * than one cell is a pointer to its cells, row by row. After a step, m.name[k]
 * is name_(n-k) for each sequence: r, s and u.
]] [[CTL_API_ void ctl_step(ctl* m_, double r_in_, const double s_in_[2]) {
    ++m_->index_;
    m_->r[0] = r_in_;
    memcpy(m_->s[0], s_in_, sizeof m_->s[0]);
]])
holds(compile_inputs_turn turn.h [[TURN_API_ void turn_init(turn* m_) {
    memset(m_, 0, sizeof *m_);
    m_->x[0][0][0] = 1.0;
    m_->x[0][1][0] = -1.0;
    m_->index_ = -1;
    turn_update(m_);
}
]] [[TURN_API_ void turn_step(turn* m_, const double x_in_[2]) {
    ++m_->index_;
    memcpy(m_->x[1], m_->x[0], sizeof m_->x[1]);
    memcpy(m_->x[0], x_in_, sizeof m_->x[0]);
]])
holds(compile_inputs_avg avg.h [[ Compiled in as constants, these
 * cannot change: d.
]])

# A parameter compiled in need not be a size, a bound or a lag: a matrix
# power's exponent and a cell's place are neither (C129).
file(WRITE "${OUT}/pw.ink" "pw(k = 2, i = 1, x_n) = {\n    y_n = ([1 1; 0 1]^k*[x_n; 1])[i]\n}\n")
set(args --compile pw.ink pw -o pw.h)
check(compile_fixed_power)
holds(compile_fixed_power pw.h [[ * name_(n-k) for each sequence: x and y. Compiled in as constants, these
 * cannot change: i and k.
 */
]])

# Refused: a history of another size than the input, a default and an
# instance within a model of another size than the model states, and a size
# that reads the index, which the interpreter refuses as it reads it.
function(unsized model text why)
    file(WRITE "${OUT}/${model}.ink" "${text}")
    set(args --compile ${model}.ink ${model} -o ${model}.h)
    set(stderr "inkamath: ${why}\n")
    set(exit 1)
    check(inputs_${model})
endfunction()
set(dot "dot(x_n[j<=2]) = {\n    y_n = [1 2]*x_n\n}\n")
unsized(broad "broad(x_n[j<=2]) = {\n    x_n | n < 0 = 0\n    c_n = x_(n-1)\n}\n"
        "cannot compile x: a history of another shape")
unsized(nil "nil(x_n[j<=2] = 0) = {\n    y_n = [1 2]*x_n\n}\n"
        "cannot compile x: a single value, where nil takes a 2x1 matrix")
unsized(lone "${dot}lone(u_n) = {\n    inner = dot(x_n = u_n)\n    y_n = inner.y_n\n}\n"
        "cannot compile inner.x: a single value, where dot takes a 2x1 matrix")
unsized(grow "grow(x_n[j<=n+1]) = {\n    y_n = x_n[1]\n}\n"
        "grow.ink, line 1: x is an input of grow, so its size cannot read the index n")
file(WRITE "${OUT}/single.ink" "${dot}v = dot(x_n = n)\n")
set(args --check single.ink v -o single.c)
set(stderr "inkamath: v.x_(0): v.x_0 is a single value, where dot takes a 2x1 matrix\n")
set(exit 1)
check(check_inputs_single)

# A size its signature names, measured where it is called, is refused there in
# the interpreter's words, a call's shapes being static (DESIGN.md, a size
# bound by a signature).
file(WRITE "${OUT}/sizes.ink" "tr(M[j<=n, k<=n]) = sum_(j=1)^n M[j,j]\nw_n = tr([n, 1, 2; 3, 4, 5])\n")
set(args --compile sizes.ink)
set(stdout "cannot compile w: tr takes M[j<=n, k<=n], not a 2x3 matrix\n")
set(exit 1)
check(compile_signature_refused)

# A sum up to the index compiled as a running total (DESIGN.md, a cumulative
# sum compiled as an accumulator): each twin of compile/accum.ink held to its
# sum, each model and its recurrence by hand compiled, reg's and every's
# totals as specified, and the refusals; part 3 checks an instance of each to
# its recurrence's report but for the name and the lines of run, ol at 300
# steps in double and in float.
set(story "${CMAKE_CURRENT_LIST_DIR}/compile/accum.ink")
# Each instance is its twin term for term in the interpreter, up to the
# steps it is checked at. A twin that is not, R_0 = 0 for e_0 in
# recreg, checks as its sum does, and the reports below cannot tell.
foreach(term IN ITEMS m.R ol.R ag.v sv.S ls.G ls.b ev.y dv.R dv.D dv.A mm.mo ey.y hy.R fy.y ms.k.S ms.y)
    string(REGEX REPLACE "\\..*" "" instance "${term}")
    set(last 99)
    if(instance STREQUAL "ol")
        set(last 299)
    endif()
    file(WRITE "${OUT}/twin.in" "prod_(k=0)^${last} (${term}_k == rec${term}_k)\n")
    execute_process(COMMAND "${EXE}" "${story}" -i INPUT_FILE "${OUT}/twin.in"
                    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code
                    TIMEOUT ${CASE_TIMEOUT})
    string(REGEX MATCH "[^\n]*\n$" answer "${out}")
    if(NOT "${code}" STREQUAL "0" OR NOT "${answer}" STREQUAL "1\n")
        message(FATAL_ERROR "compile_accum_twin: ${term} is not rec${term} up to ${last}: ${answer}${err}")
    endif()
endforeach()
foreach(model IN ITEMS reg ogd ada surv gram every dr moments early hist fwd outer)
    foreach(written IN ITEMS ${model} rec${model})
        set(args --compile ${story} ${written} -o ${written}.h)
        check(compile_accum_${written})
        strict(compile_accum_${written} ${written}.h)
    endforeach()
endforeach()
holds(compile_accum_reg reg.h "    double s0_[2];\n"
      "    m_->s0_[0] = (double)m_->index_ < 0.0 ? 0.0 : m_->s0_[1] + m_->e[0];\n"
      "    m_->R[0] = m_->s0_[0];\n")
holds(compile_accum_every every.h "    double s0_[2];\n"
      "    m_->s0_[0] = (double)m_->index_ < 1.0 ? 0.0 : m_->s0_[1] + m_->x[1];\n"
      "    m_->y[0] = floor((double)m_->index_ / 2.0) * 2.0 == (double)m_->index_ ? m_->s0_[0] : -1.0;\n")

set(refused_adam "v: a sum up to t whose terms read t; write it as a recurrence")
set(refused_square "y: a sum up to n whose terms read n; write it as a recurrence")
set(refused_peak "M: a maximum up to n, for now")
set(refused_peakat "A: an argmax up to n, for now")
set(neither "y: a sum whose last index is neither a constant nor n less a constant")
set(refused_ahead "${neither}")
set(refused_twice "${neither}")
set(refused_nested "y: a sum up to an enclosing sum's index, for now")
set(refused_window "y: a sum whose first index is not a constant")
set(refused_late "R: R_0 reads x_-1, before the stream, where x has no history")
set(refused_earlier "R: R_0 reads R's sum up to -1, before the stream, where it gives none: x_-1 is an input, and nothing defines it")
set(refused_gramfrom "G: a sum up to n empty at 0, a single 0 there and a 2x2 matrix after")
set(refused_slope "g: a sum up to n under grad, for now")
set(refused_slow "y: a sum up to m in a sequence computed every 2, for now")
set(refused_slowafter "${refused_slow}")
foreach(model IN ITEMS adam square peak peakat ahead twice nested window late earlier gramfrom slope
                       slow slowafter)
    set(args --compile ${story} ${model})
    set(stdout "cannot compile ${refused_${model}}\n")
    set(exit 1)
    check(compile_accum_${model})
endforeach()

if(PART EQUAL 1)
    return()
endif()
endif()

if(NOT PART OR PART LESS 3)

# The prelude's charpoly compiles, and rho, walked (compile/seqcells.ink), of
# a 1x1 matrix too, where its sizes went unbound, "n is not defined" (C213);
# grad refuses it and abscissa where A moves, as the interpreter does
# (DESIGN.md, the characteristic polynomial and stability).
file(WRITE "${OUT}/charpoly.ink" "x_n = rho([n 1; -1 1/2])\nz_n = charpoly([n 1; 2 3])[2]\ng_n = grad_(a = n) rho([a 1; -1 1/2])\nh_n = grad_(a = n) abscissa(a)\ns_n = rho([1/(n+2)])\n")
set(args --compile charpoly.ink)
set(stdout "cannot compile g: grad cannot differentiate rho yet\ncannot compile h: grad cannot differentiate abscissa yet\n")
set(exit 1)
check(compile_charpoly_refused)

# C443: a call of constants, or its limit, folds as the interpreter answers
# it, before its body is compiled, which refused rho, schurcohn, hurwitz,
# roots and h by their sequences with parameters by cells. One that reads a
# value, which may move, is compiled, and one the interpreter refuses is
# refused in its words.
file(WRITE "${OUT}/c443.ink" "G(a, b) = [a, b; 0, a/2]\npair(a, b) = [a, b/4; b/8, a]\nh(q)_0 = q\nh(q)_k[j<=2] = h(q)_(k-1)[j]/2\nc = [1; 2]\nf(x) = c[1]*x\nx_n = rho(G(1/2, 1)) + n\ny_n = schurcohn(charpoly(pair(1/2, 1)))*n\nz_n = hurwitz([1; 3; 2]) - hurwitz([1; -3; 2]) + n\ne_n = eig(pair(1, 1)*pair(1, 1)')[2] + n\nu_n = h([1; 2])_3[2] + n\ng_n = grad_(t = n) t^2*abscissa(G(-1/2, 1))\nk_n = grad_(t = 1/2) G(t, t)[1,2]*n\nr_n = roots([1; -3; 2])[2] + n\nl_n = [0, 1]*lim h([1; 2]) + n\nv_n = f(4) + n\nw_n = f(3) + n\n")
set(args --compile c443.ink -o c443.h)
check(compile_c443)
holds(compile_c443 c443.h "    m_->x[0] = 0.5 + (double)m_->index_;\n"
      "    m_->y[0] = 1.0 * (double)m_->index_;\n" "    m_->z[0] = 1.0 + (double)m_->index_;\n"
      "    m_->u[0] = 0.25 + (double)m_->index_;\n"
      "    m_->g[0] = 2.0 * pow((double)m_->index_, 1.0) * 1.0 * -0.25;\n"
      "    m_->k[0] = 1.0 * (double)m_->index_;\n"
      "    m_->r[0] = 2.0 + (double)m_->index_;\n"
      "    m_->l[0] = 5.820766091346741e-11 + (double)m_->index_;\n"
      "    m_->v[0] = m_->c[0][0] * 4.0 + (double)m_->index_;\n"
      "    m_->w[0] = m_->c[0][0] * 3.0 + (double)m_->index_;\n")
file(WRITE "${OUT}/c443m.ink" "m(a = 1) = {\n    v = a*2\n}\ng = m(a = 3)\nf(x) = g.a*x\nw_n = f(2) + n\n")
set(args --compile c443m.ink -o c443m.h)
check(compile_c443_member)
holds(compile_c443_member c443m.h "    m_->w[0] = m_->g.a * 2.0 + (double)m_->index_;\n")
set(args --check c443.ink -o c443.c)
check(check_c443)
reports(check_c443 c443 "c443.ink: 100 steps from 0, against exact values until 0 and inexact ones from there
e: within 0; the interpreter's terms about 1.4e-14 from the exact ones
g: within 0\nk: within 0
l: within 0; the interpreter's terms about 5.8e-11 from the exact ones
r: within 0\nu: within 0\nv: within 0\nw: within 0\nx: within 0\ny: within 0\nz: within 0\n")
file(WRITE "${OUT}/c443r.ink" "r_n = eig([0, 1; -1, 0])[1] + n\ns_n = rho([~1e308, ~1e308; ~1e308, ~1e308]*10) + n\n")
set(args --compile c443r.ink)
set(stdout "cannot compile r: eig needs a matrix whose eigenvalues are all real\ncannot compile s: rho needs finite cells, not ~inf\n")
set(exit 1)
check(compile_c443_refused)
# C444: a guarded recursion whose argument's size falls below 1 where no
# input takes it, abscissaq's, is NaN there, not refused.
file(WRITE "${OUT}/c444.ink" "f(p[j<=m]) = 1\nf(p[j<=m]) | p[m] == 0 = f(t(p))\nt(p[j<=m])[j<=m-1] = p[j]\ny_n = f([1; n - 3])\n")
set(args --check c444.ink -o c444.c)
check(check_c444)
reports(check_c444 c444 "c444.ink: 100 steps from 0, against exact values\ny: within 0\n")
# C446: grad's name is not the name another file's definition reads.
set(args "${CMAKE_CURRENT_LIST_DIR}/compile/flowgrad.ink" -i)
set(input "y_2\n")
set(stdout "use flow\ncost(W, a) = a*flow.total(W)\ny_n = grad_(v = n) cost(v, v)\n12\n")
check(c446)
set(args --check "${CMAKE_CURRENT_LIST_DIR}/compile/flowgrad.ink" -o c446.c)
check(check_c446)
reports(check_c446 c446 "flowgrad.ink: 100 steps from 0, against exact values\ny: within 0\n")

# check_not_transcript reads the models.ink part 1 writes.
file(WRITE "${OUT}/models.ink" "gain(k = 2, x_n) = { y_n = k*x_n + z_n }\n")

# Nor do hinf and dhinf, their hurwitz walked: hinfl's hinfo(u, m), by cells
# and its bound reading m, is a function of the header whose m is a double,
# as hinfo(x_n, 1) alone is refused (C445); grad refuses them where A moves
# (DESIGN.md, the H-infinity norm). Of a 1x1 matrix, dhinf's sizes went unbound, "n is not defined",
# and hinf's sequence with parameters was "in a limit's terms", where there is
# none (C213).
file(WRITE "${OUT}/hinf.ink" "x_t = hinf(-t-1, 1, 1)\ng_t = grad_(a = t) hinf(-a-1, 1, 1)\nh_t = grad_(a = t) dhinf(1/(a+2), 1, 1)\nd_t = dhinf(1/(t+2), 1, 1)\n")
set(args --compile hinf.ink)
set(stdout "cannot compile d: a matrix whose size is not a constant\ncannot compile g: grad cannot differentiate hinf yet\ncannot compile h: grad cannot differentiate dhinf yet\ncannot compile x: a matrix whose size is not a constant\n")
set(exit 1)
check(compile_hinf_refused)

# A tensor input, refused before tensors compiled, read by its second slice.
file(WRITE "${OUT}/batch.ink" "batch(x_n[b<=2, j<=1, k<=2]) = {\n    y_n = x_n[2]*[1; 1]\n}\n")
set(args --compile batch.ink batch -o batch.h)
check(inputs_batch)
holds(inputs_batch batch.h "    m_->y[0] = m_->x[0][1][0][0] * 1.0 + m_->x[0][1][0][1] * 1.0;\n")

# A model's history of its inputs (DESIGN.md): a read before the stream that
# no history gives, and a history that init cannot fold, each refused by name.
function(refused model body why)
    file(WRITE "${OUT}/${model}.ink" "${model}(${body}\n}\n")
    set(args --compile ${model}.ink ${model} -o ${model}.h)
    set(stderr "inkamath: cannot compile ${why}\n")
    set(exit 1)
    check(history_${model})
endfunction()
refused(open "x_n) = {\n    c_n = x_(n-1)"
        "c: c_0 reads x_-1, before the stream, where x has no history")
refused(scant "x_n) = {\n    x_(-1) = 0\n    c_n = x_(n-2)"
        "c: c_0 reads x_-2, before the stream, where x has no history")
refused(level "x_n) = {\n    c_n = n >= 0 and x_n > x_(n-1)"
        "c: c_0 reads x_-1, before the stream, where x has no history")
refused(half "u_n, v_n) = {\n    u_n | n < 0 = 0\n    c_n = u_(n-1) + v_(n-1)"
        "c: c_0 reads v_-1, before the stream, where v has no history")
refused(kept "x_n) = {\n    c_n | x_n > 0 = x_(n-1)\n    c_n = 0"
        "c: c_0 reads x_-1, before the stream, where x has no history")
refused(held "x_n) = {\n    y_m = x_(2*m)\n    z_n = y_(floor(n/2) - 1)"
        "z: z_0 reads y_-1, before y's first tick, where its samples could give a term")
refused(early "x_n) = {\n    x_n | n < 2 = 0\n    c_n = x_(n-1)"
        "x: its history reaches x_0, in the stream")
refused(even "x_n) = {\n    x_n | n^2 > 4 = 0\n    c_n = x_(n-3)"
        "x: a history whose guard is not its index below a constant")
refused(mute "u_n, x_n) = {\n    x_n | n < 0 and u_n > 0 = 0\n    c_n = x_(n-1)"
        "x: a history whose guard is not its index below a constant")
refused(biased "a = 1, x_n) = {\n    x_n | n < 0 = a\n    c_n = x_(n-1)"
        "x: a history that reads a")
refused(swap "x_n) = {\n    x_n | n < 0 = [0; 0]\n    c_n = [0 1; 1 0]*x_(n-1)"
        "x: a history of another shape")

# A slow sequence's base clause, which the stream starts after, reading the
# input before it.
refused(planted "x_n) = {\n    y_2 = x_1\n    y_m = x_(2*m)\n    z_n = y_(floor(n/2))"
        "y: y_2 reads x_1, before the stream, where x has no history")

# A history that gives no term, or none a double holds, where it is read.
refused(singular "x_n) = {\n    x_n | n < 0 = 1/(n+1)\n    c_n = x_(n-1)"
        "c: c_0 reads x_-1, before the stream, where x's history gives none: division by zero")
refused(vast "x_n) = {\n    x_n | n < 0 = 10^400\n    c_n = x_(n-1)"
        "c: c_0 reads x_-1, before the stream, where x's history gives a term no double holds")

# A term init would fold from a history but that reads a parameter, which the
# host may assign after init: through a slow sequence's samples, and through
# an argument.
refused(scaled "a = 2, x_n) = {\n    x_n | n < 0 = 1\n    y_m = a*x_(2*m)\n    z_n = y_(floor(n/2) - 1)"
        "z: z_0 reads y_-1, before y's first tick, where its samples could give a term")
file(WRITE "${OUT}/gained.ink" "two(x_n) = {\n    x_(-2) = 0\n    c_n = x_(n-1) + x_(n-2)\n}\n"
     "gained(a = 2, x_n) = {\n    x_n | n < 0 = 3\n    inner = two(x_n = a*x_n)\n"
     "    c_n = inner.c_n\n}\n")
set(args --compile gained.ink gained -o gained.h)
set(stderr "inkamath: cannot compile inner.c: inner.c_0 reads inner.x_-1, before the stream, where inner.x's history reads a, which the host may assign after init\n")
set(exit 1)
check(history_gained)
# But not where the history's own clause gives the term: 'rail' reads u_-1,
# 1/2, where its argument reads a parameter, and was refused (C379).
file(WRITE "${OUT}/railed.ink" "steering(k = 2, y_n) = {\n    u_n = k*(1 - y_n)\n}\n"
     "railing(u_n) = {\n    u_n | n < 0 = 1/2\n    x_0 = 0\n    a_n = u_(n-1) - x_n\n"
     "    x_n = x_(n-1) + a_(n-1)/2\n}\nsteer = steering(y_n = rail.x_n)\n"
     "rail = railing(u_n = steer.u_n)\n")
set(args --compile railed.ink -o railed.h)
check(history_railed)
holds(history_railed railed.h "m_->rail.u[0] = 0.5;\n")
# And a recurrence's own history that reads one, which the step read as 0
# (C343).
refused(lifted "a = 2) = {\n    g_n | n < 0 = a\n    g_n = 2*g_(n-1)"
        "g: g_0 reads g_-1, before the stream, where g's history reads a, which the host may assign after init")
# And one with no default, which init cannot evaluate.
refused(unset "a) = {\n    g_n | n < 0 = a\n    g_n = 2*g_(n-1)"
        "g: g_0 reads g_-1, before the stream, where g's history reads a, which the host may assign after init")
# And at another rate, where it was read as 0.
refused(slowed "a = 2, x_n) = {\n    y_m | m < 0 = a\n    y_m = y_(m-1) + x_(2*m)"
        "y: y_0 reads y_-1, before the stream, where y's history reads a, which the host may assign after init")
# C364: a cycle of three, its terms before the stream its guards', reading an
# input of no history: c, compiled before a's read of b was recorded, was not
# taken for a recurrence, and computed again at -1 read x_-1.
file(WRITE "${OUT}/cycled.ink" "cycling(x_n) = {\n    a_n | n < 0 = 1\n    a_n = b_(n-1)/2\n"
     "    b_n | n < 0 = 1\n    b_n = c_(n-1)/2\n    c_n | n < 0 = 1\n    c_n = a_(n-1)/2 + x_n\n}\n")
set(args --compile cycled.ink cycling)
set(stdout "cycling in cycled.ink compiles\n")
check(history_cycled)
# C403: a cycle guarded on some of its sequences only, whose term before the
# stream reads the input, is refused for it rather than for having no history.
refused(reached "x_n) = {\n    a_n | n < 0 = 1\n    a_n = b_(n-1)/2\n    b_n = a_(n-1)/2 + x_n"
        "a: a_0 reads b_-1, before the stream, where b's history gives none: x_-1 is an input, and nothing defines it")
# And a long one, each term computed again at each read, read twice: 7.6 s at
# 14, past 100 at 18.
set(text "long(x_n) = {\n    s0_n | n < 0 = 1\n")
foreach(i RANGE 17)
    math(EXPR next "(${i} + 1) % 18")
    string(APPEND text "    s${i}_n = (s${next}_(n-1) + s${next}_(n-2))/4\n")
endforeach()
file(WRITE "${OUT}/long.ink" "${text}}\n")
set(args --compile long.ink long)
set(stdout "long in long.ink compiles\n")
check(history_cycled_long)
# Init folds a slow history back from its first term, so a term it reads
# past one it cannot fold is refused for that one (C348).
refused(gapped "a = 2, x_n) = {\n    y_m | m < -1 = 1\n    y_m | m < 0 = a\n    y_m = y_(m-2) + x_(2*m)"
        "y: y_1 reads y_-1, before the stream, where y's history reads a, which the host may assign after init")
# And below a slow sequence's base clause, where it was read as 0 (C344),
# with or without a guard.
refused(sunk "a = 2, x_n) = {\n    y_0 = 1\n    y_m | m < 0 = a\n    y_m = y_(m-2) + x_(2*m)"
        "y: y_1 reads y_-1, below y's base clauses, where only its guards could give a term")
refused(rootless "x_n) = {\n    y_0 = 1\n    y_m = y_(m-2) + x_(2*m)"
        "y: y_1 reads y_-1, before it starts at 0")

# A history of a single value for an inner input given a matrix.
file(WRITE "${OUT}/stacked.ink" "lag(x_n) = {\n    x_n | n < 0 = 0\n    c_n = x_(n-1)\n}\n"
     "stacked(x_n) = {\n    x_n | n < 0 = 0\n    inner = lag(x_n = [x_n; x_n])\n"
     "    c_n = inner.c_n\n}\n")
set(args --compile stacked.ink stacked -o stacked.h)
set(stderr "inkamath: cannot compile inner.x: a history of another shape\n")
set(exit 1)
check(history_shape)

# A guarded clause's value is read only where its guard, folded, holds.
file(WRITE "${OUT}/guarded.ink" "guarded(x_n) = {\n    c_n | n > 0 = x_(n-1)\n    c_n = 0\n}\n")
set(args --compile guarded.ink guarded -o guarded.h)
check(history_guarded_value)

# A read on the right of an 'and' whose left does not decide is a term the
# history gives, as one read on its left is.
file(WRITE "${OUT}/gate.ink" "gate(x_n) = {\n    x_n | n < 0 = 5\n    c_n = n < 4 and x_(n-3) > 0\n}\n")
set(args --compile gate.ink gate -o gate.h)
check(history_right_side)

# A transcript checked: replayed, and each answer that is not the one recorded
# shown as recorded, '-', and as given now, '+', under the line it answers.
file(WRITE "${OUT}/good.ink" ">> 1+1\n2\n\n# a comment\n>> a = 3\na = 3\n\n>> a*2\n6\n")
set(args --check good.ink)
set(stdout "good.ink: 3 answers, each as recorded\n")
check(check_transcript)

file(WRITE "${OUT}/stale.ink" ">> 1+1\n3\n\n>> [1, 2;\n.. 3, 4]\n[1, 2;\n 3, 5]\n\n>> 2*2\n4\n")
set(args --check stale.ink)
set(stdout "stale.ink:1: >> 1+1\n- 3\n+ 2\nstale.ink:4: >> [1, 2;\n.. 3, 4]\n- [1, 2;\n-  3, 5]\n+ [1, 2;\n+  3, 4]\nstale.ink: 2 of 3 answers are not those recorded\n")
set(exit 1)
check(check_transcript_stale)

set(args --check models.ink)
set(stderr "inkamath: models.ink is not a transcript\n")
set(exit 1)
check(check_not_transcript)


# A float target (DESIGN.md, compile/float.ink): beside --compile or a
# --check that writes a program, floats where the header writes doubles.
set(args --float first.txt)
set(stderr "inkamath: --float takes --compile, or --check with -o check.c\nTry 'inkamath --help'.\n")
set(exit 2)
check(float_usage)

file(MAKE_DIRECTORY "${OUT}/float")
set(args --compile ${inputs} dot --float -o float/dot.h)
check(float_dot)
holds(float_dot float/dot.h [[typedef struct dot {
    long long index_;
    float x[1][2][1];
    float y[1];
} dot;
]] [[DOT_API_ void dot_step(dot* m_, const float x_in_[2]) {
]])

file(WRITE "${OUT}/vast.ink" "vast(x_n) = {\n    x_n | n < 0 = 10^39\n    c_n = x_(n-1)\n}\n")
set(args --compile vast.ink vast --float)
set(stdout "cannot compile c: c_0 reads x_-1, before the stream, where x's history gives a term no float holds\n")
set(exit 1)
check(float_history)

# A float's limit stops where it closes on floats a unit or two apart, too.
set(args --compile ${CMAKE_CURRENT_LIST_DIR}/compile/logistic.ink logit --float -o float/gate.h)
check(float_gate)
holds(float_gate float/gate.h "#include <float.h>\n#include <math.h>\n"
      [[static inline float gate_lim0(const gate* m_, int ran_, float arg_z) {]]
      [[        const float t_ = t1_ + (t1_ + (0.0f - t2_)) * arg_z / (float)k_;
        if (isnan(t_)) return ran_ ? 0.0f : held_ ? kept_ : NAN;
        if (started_) {
            step_ = fabsf(t_ - t1_);
            close_ = step_ == 0 ? 0.0f : step_ / fabsf(t_);
            step_ = step_ / fmaxf(1.0f, fabsf(t_));
            if (step_ > 0 && step_ <= 2 * FLT_EPSILON * fabsf(t_) / fmaxf(1.0f, fabsf(t_)) && isfinite(t_) && stepped_) return ran_ ? 0.0f : t_;
            if (close_ <= 1e-10f && stepped_ &&
]] [[    m_->p[0][0][0] = 1.0f / (1.0f + gate_lim0(m_, 0, 0.0f + (0.0f - (0.0f * m_->w[0][0][0] + 0.0f * m_->w[0][1][0] + m_->b[0]))));
]])

set(args --check ${CMAKE_CURRENT_LIST_DIR}/compile/drift.ink calm --float -o float/calm.c)
check(float_calm)
holds(float_calm float/calm.c [[static const float in_0[100] = {
    0.0f, 0.1f, 0.2f, 0.3f,
]] [[static float got_0[100];
]] [[        checked_step(&m_, in_0[n_]);
        memcpy(&got_0[n_ * 1], &m_.v[0], sizeof(float) * 1);
]])

# C103: a parameter's default of negative infinity is -INFINITY, as is one
# past a float's range in a float header.
file(WRITE "${OUT}/c103.ink" "neg(p = 0 - ~(10^400), q = 0 - 10^39, x_n) = {\n    y_n = p + q + x_n\n}\n")
set(args --compile c103.ink neg -o c103.h)
check(compile_c103)
holds(compile_c103 c103.h "    m_->p = -INFINITY;\n    m_->q = -1e+39;\n")
set(args --compile c103.ink neg --float -o c103f.h)
check(compile_c103_float)
holds(compile_c103_float c103f.h "    m_->p = -INFINITY;\n    m_->q = -INFINITY;\n")

# C104: a header named after a word C keeps, or after a standard header it
# includes, is refused; a program is not named after its file (C346).
set(args --compile ${inputs} dot --float -o float.h)
set(stderr "inkamath: 'float' is a name C keeps, and the header is named after it\n")
set(exit 2)
check(compile_c104)
set(args --check ${CMAKE_CURRENT_LIST_DIR}/compile/drift.ink calm -o math.c)
check(check_c104)
holds(check_c104 math.c "    checked_init(&m_);\n")
# C347: and a name a header it includes keeps for a macro.
file(WRITE "${OUT}/c347.ink" "NAN_n = n\n")
set(args --compile c347.ink -o c347.h)
set(stderr "inkamath: cannot compile NAN: NAN is a word C keeps for itself\n")
set(exit 1)
check(compile_c347)
# C400: and a word C++ keeps, the headers building as C++ too (C393), in each
# place a name reaches one; a model's own name reaches none.
file(WRITE "${OUT}/c400.ink" "par(x_n, new = 1) = { y_n = new*x_n }\n"
     "seq(x_n) = { class_n = 2*x_n }\ninp(this_n) = { y_n = this_n }\n"
     "template(x_n) = { y_n = 2*x_n }\nof(x_n) = { friend = template(x_n = x_n) }\n")
foreach(case "par:new:new" "seq:class:class" "of:friend:friend.x")
    string(REPLACE ":" ";" case "${case}")
    list(POP_FRONT case model word term)
    set(args --compile c400.ink ${model} -o c400.h)
    set(stderr "inkamath: cannot compile ${term}: ${word} is a word C++ keeps for itself\n")
    set(exit 1)
    check(compile_c400_${model})
endforeach()
set(args --compile c400.ink inp -o c400.h)
set(stderr "inkamath: this is a word C++ keeps for itself\n")
set(exit 1)
check(compile_c400_inp)
set(args --compile c400.ink template -o c400.h)
check(compile_c400)
strict(compile_c400 c400.h)
set(args --compile c400.ink template -o template.h)
set(stderr "inkamath: 'template' is a name C++ keeps, and the header is named after it\n")
set(exit 2)
check(compile_c400_named)

# grad compiled (DESIGN.md, compile/grad.ink): the gradient written out, each
# function of the prelude called on a value that moves with one for its part,
# and what stays refused named in the interpreter's words where it has them.
set(grad "${CMAKE_CURRENT_LIST_DIR}/compile/grad.ink")
set(args --compile ${grad} lsq -o lsq.h)
check(compile_grad_line)
holds(compile_grad_line lsq.h [[    const double t0_ = 1.0 * m_->w[1][0][0] + 0.0 * m_->w[1][1][0] - 1.0;
    const double t1_ = 2.0 * pow(t0_, 1.0);
]] [[    const double t11_ = 0.125 * t1_;
    m_->w[0][0][0] = m_->index_ == 0 ? 0.0 : m_->w[1][0][0] + (0.0 - m_->eta * (1.0 * t8_ + 1.0 * t9_ + 1.0 * t10_ + 1.0 * t11_));
    m_->w[0][1][0] = m_->index_ == 0 ? 0.0 : m_->w[1][1][0] + (0.0 - m_->eta * (3.0 * t8_ + 2.0 * t9_ + 1.0 * t10_ + 0.0 * t11_));
]])
set(args --compile ${grad} descent -o fall.h)
check(compile_grad_fall)
holds(compile_grad_fall fall.h [[static inline double fall_exp_dx(double arg_x, double part_x) {
    return isnan(arg_x) ? NAN : arg_x > 1000.0 ? 0.0 : isnan(arg_x) ? NAN : arg_x < -1000.0 ? 0.0 : fall_expk_dx(arg_x, (floor(arg_x * 1.4426950408889634 + 0.5) == arg_x * 1.4426950408889634 + 0.5 ? NAN : floor(arg_x * 1.4426950408889634 + 0.5)), part_x);
}
]] [[static inline double fall_logs_ds(double arg_s, double part_s) {
    return 2.0 * part_s * fall_logp(arg_s * arg_s) + 2.0 * arg_s * fall_logp_dz(arg_s * arg_s, part_s * arg_s + arg_s * part_s);
}
]])
file(WRITE "${OUT}/grad_refused.ink" "c_n = grad_(v = [x_n; 1]) 2*v\nd_n = grad_(t = x_n) 2^t\nf_n = grad_(t = x_n) t^x_n\nh_n = grad_(t = x_n) 5\nk_n = grad_(t = x_n) sq\nm_n = grad_(t = x_n) amp(k = t).y\namp(k = 1) = {\n    y = 2*k\n}\nsq = t^2\nt = 3\n")
set(args --compile grad_refused.ink)
set(stdout "cannot compile c: grad of a matrix with respect to a matrix is a Jacobian, which it does not give\ncannot compile d: grad cannot differentiate a power whose exponent changes with t, unless its base is e\ncannot compile f: a derivative of a power whose exponent is not a constant, for now\ncannot compile h: grad's expression does not read t\ncannot compile k: sq reads the global t, which grad's t does not reach\ncannot compile m: grad cannot differentiate through an instance yet\n")
set(exit 1)
check(compile_grad_refused)
# A derivative of a limit compiled (DESIGN.md, compile/limgrad.ink): each
# model compiled, strict, and what stays refused in its words; its
# instances are test/CMakeLists.txt's checks.
set(limgrad "${CMAKE_CURRENT_LIST_DIR}/compile/limgrad.ink")
foreach(model IN ITEMS walks zero ended rooted fails constant beside kinks)
    set(args --compile ${limgrad} ${model} -o limgrad_${model}.h)
    check(compile_limgrad_${model})
    strict(compile_limgrad_${model} limgrad_${model}.h)
endforeach()
set(args --compile ${limgrad} walks --float -o limgrad_float.h)
check(compile_limgrad_float)
strict(compile_limgrad_float limgrad_float.h)

# a to d, g, j and m step with t at a comparison or a floor, as C215
# refuses them, g's in mod, j's in ilogb, which a session answers at
# ~0.3 and refuses at 3/10, and m's in exp's argument; d is t by its binary digits, which the
# interpreter answers 0 where it is 1; f and h are second derivatives,
# h's by a grad in the terms. A Hessian of a limit, and a power at a
# sum's index whose base moves, are compile_hessian_refused's c and
# compile_sums_refused's d.
file(WRITE "${OUT}/limgrad_refused.ink"
     "a_n = grad_(t = x_n) lim bis(t)\nb_n = grad_(t = x_n) lim fl(t)\n"
     "c_n = grad_(t = x_n) lim gd(t)\n"
     "d_n = grad_(t = x_n) sum_(q=1) (floor(2^q*t) - 2*floor(2^(q-1)*t))/2^q\n"
     "f_n = grad_(s = x_n) grad_(t = s) s*lim p(t)\n"
     "g_n = grad_(t = x_n) lim md(t)\nh_n = grad_(t = x_n) lim nw(t)\n"
     "j_n = grad_(t = x_n) lim lb(t)\nk_n = grad_(t = x_n) lim tp(t)\n"
     "m_n = grad_(t = x_n) lim ef(t)\n"
     "ef(x)_0 = 0\nef(x)_k = ef(x)_(k-1)/2 + exp(floor(2^k*x)/2^k)\n"
     "bis(x)_0 = [0; 1]\nbis(x)_n = bis(x)_(n-1) + (bis(x)_(n-1)[2] - bis(x)_(n-1)[1])/2*"
     "([1; 0] - ((bis(x)_(n-1)[1] + bis(x)_(n-1)[2])/2 > x)*[1; 1])\n"
     "fl(x)_n = floor(10^n*x)/10^n\n"
     "md(x)_0 = 0\nmd(x)_k = md(x)_(k-1)/2 + mod(x, 1)\n"
     "lb(x)_0 = 0\nlb(x)_k = lb(x)_(k-1)/2 + ilogb(x)\n"
     "gd(x)_0 = 0\ngd(x)_k | x > 1 = gd(x)_(k-1)/2 + 1\ngd(x)_k = gd(x)_(k-1)/2 + x\n"
     "p(r)_0 = 1\np(r)_k = r*p(r)_(k-1)/4 + 1\n"
     "nw(x)_0 = 1\nnw(x)_k = nw(x)_(k-1) - (nw(x)_(k-1)^2 - x)/grad_(v = nw(x)_(k-1)) v^2\n"
     "tp(x)_0 = [1;; 0]\ntp(x)_k = tp(x)_(k-1)*x/2 + [1;; x]\n")
set(args --compile limgrad_refused.ink)
set(stepped ", whose terms step with t at a comparison or a floor\n")
set(second ": a second derivative through a limit, for now\n")
string(CONCAT stdout "cannot compile a: grad cannot differentiate lim bis${stepped}"
              "cannot compile b: grad cannot differentiate lim fl${stepped}"
              "cannot compile c: grad cannot differentiate lim gd${stepped}"
              "cannot compile d: grad cannot differentiate sum_(q=1)${stepped}"
              "cannot compile f${second}"
              "cannot compile g: grad cannot differentiate lim md${stepped}"
              "cannot compile h${second}"
              "cannot compile j: grad cannot differentiate lim lb${stepped}"
              "cannot compile k: a tensor in a limit, for now\n"
              "cannot compile m: grad cannot differentiate lim ef${stepped}")
set(exit 1)
check(compile_limgrad_refused)

# A matrix inverse and power under grad compiled (DESIGN.md,
# compile/matpow.ink): each model strict, one in floats, and what stays
# refused in the words of the same power outside grad.
set(matpow "${CMAKE_CURRENT_LIST_DIR}/compile/matpow.ink")
foreach(model IN ITEMS exact arm twice names sweep pole)
    set(args --compile ${matpow} ${model} -o matpow_${model}.h)
    check(compile_matpow_${model})
    strict(compile_matpow_${model} matpow_${model}.h)
endforeach()
set(args --compile ${matpow} arm --float -o matpow_farm.h)
check(compile_matpow_float)
strict(compile_matpow_float matpow_farm.h)
file(WRITE "${OUT}/matpow_refused.ink"
     "a_n = grad_(t = x_n) [t 1; 0 t]^x_n\nb_n = grad_(t = x_n) [t 1; 0 t]^(1/2)\n"
     "c_n = grad_(t = 1) [1 t; t 1]^-1 + x_n\nd_n = grad_(t = x_n)^3 [t 1; 0 1]^-1\n"
     "f_n = grad_(t = x_n) [t 1; 0 1;; 1 0; 0 t]^2\nh_n = grad_(t = x_n) [t 1 0; 0 t 1]^2\n"
     "q_n = grad_(t = x_n) [t 1; 0 t]^2147483648\n")
set(args --compile matpow_refused.ink)
set(stdout "cannot compile a: a matrix power whose exponent is not a constant
cannot compile b: a matrix power must be a whole number
cannot compile c: a singular matrix has no inverse
cannot compile d: a derivative of a derivative of a derivative, for now
cannot compile f: only a matrix has a power, not a 2x2x2 tensor
cannot compile h: only a square matrix has a power
cannot compile q: a matrix power must be a whole number
")
set(exit 1)
check(compile_matpow_refused)

# Guards on cells at run time (DESIGN.md, compile/cellguards.ink): a ReLU by
# cells computed as net.h's term by cells is, grad through cells, and what
# stays refused.
set(cellguards "${CMAKE_CURRENT_LIST_DIR}/compile/cellguards.ink")
set(args --compile ${cellguards} net -o cnet.h)
check(compile_cellguards_layer)
holds(compile_cellguards_layer cnet.h [[    m_->h[0][0][0] = m_->z[0][0][0] < 0.0 ? 0.0 : m_->z[0][0][0];
    m_->h[0][1][0] = m_->z[0][1][0] < 0.0 ? 0.0 : m_->z[0][1][0];
]])
file(WRITE "${OUT}/cel.ink" "q_n = grad_(t = x_n) cel(t)[2]\ncel(z)[j<=2] = j*z\n")
set(args --compile cel.ink -o cel.h)
check(compile_cellguards_cel)
holds(compile_cellguards_cel cel.h "    m_->q[0] = 2.0;\n")
file(WRITE "${OUT}/cellguards.ink" "a_n = grad_(v = [x_n; 1]) up(v)\nc_n[j<=2] = j*x_n\n"
     "c_2[1] | x_2 > 0 = 5\nup(z)[i] | z[i] > 0 = z[i]\nup(z)[i] = 0\nx_n = n - 1\n")
set(args --compile cellguards.ink)
set(stdout "cannot compile a: grad of a matrix with respect to a matrix is a Jacobian, which it does not give
cannot compile c: a guarded cell of one term
")
set(exit 1)
check(compile_cellguards_refused)

# Loops in generated code (DESIGN.md): compile/loops.ink's headers holding
# the loops each increment writes, a temporary's number written '#' where
# the rule does not fix it, each header built as a strict host's C.
set(loops ${CMAKE_CURRENT_LIST_DIR}/compile/loops.ink)
set(transformer ${CMAKE_CURRENT_LIST_DIR}/compile/transformer.ink)
file(MAKE_DIRECTORY "${OUT}/float")
foreach(model spin solve dense gated diag raised turn)
    set(args --compile ${loops} ${model} -o ${model}.h)
    check(compile_loops)
endforeach()

# 1, products of 27 multiplications or more, read in place, transposed,
# made an array, slice by slice; fewer written out.
holds(compile_loops_products spin.h
      [[    m_->v[0][0][0] = m_->index_ == 0 ? 1.0 : m_->B[0][0] * m_->v[1][0][0] + m_->B[0][1] * m_->v[1][1][0];
    m_->v[0][1][0] = m_->index_ == 0 ? 2.0 : m_->B[1][0] * m_->v[1][0][0] + m_->B[1][1] * m_->v[1][1][0];
]] [[    double t0_[2][3][3];
    for (int b_ = 0; b_ < 2; ++b_)
        for (int i_ = 0; i_ < 3; ++i_)
            for (int j_ = 0; j_ < 3; ++j_) {
                double s_ = m_->w[1][b_][i_][0] * m_->A[0][j_];
                for (int p_ = 1; p_ < 3; ++p_) s_ = s_ + m_->w[1][b_][i_][p_] * m_->A[p_][j_];
                t0_[b_][i_][j_] = s_;
            }
    m_->w[0][0][0][0] = m_->index_ == 0 ? 1.0 : t0_[0][0][0];
]] [[    double t1_[3][3];
    for (int i_ = 0; i_ < 3; ++i_)
        for (int j_ = 0; j_ < 3; ++j_) {
            double s_ = m_->A[i_][0] * m_->x[1][0][j_];
            for (int p_ = 1; p_ < 3; ++p_) s_ = s_ + m_->A[i_][p_] * m_->x[1][p_][j_];
            t1_[i_][j_] = s_;
        }
    m_->x[0][0][0] = m_->index_ == 0 ? 1.0 : t1_[0][0];
]] [[    double t2_[3][3];
    for (int i_ = 0; i_ < 3; ++i_)
        for (int j_ = 0; j_ < 3; ++j_) {
            double s_ = m_->A[0][i_] * m_->x[0][0][j_];
            for (int p_ = 1; p_ < 3; ++p_) s_ = s_ + m_->A[p_][i_] * m_->x[0][p_][j_];
            t2_[i_][j_] = s_;
        }
]] [[    double t3_[3][3] = {{m_->A[0][0] + m_->x[0][0][0], m_->A[0][1] + m_->x[0][0][1], m_->A[0][2] + m_->x[0][0][2]}, {m_->A[1][0] + m_->x[0][1][0], m_->A[1][1] + m_->x[0][1][1], m_->A[1][2] + m_->x[0][1][2]}, {m_->A[2][0] + m_->x[0][2][0], m_->A[2][1] + m_->x[0][2][1], m_->A[2][2] + m_->x[0][2][2]}};
    double t4_[3][3];
    for (int i_ = 0; i_ < 3; ++i_)
        for (int j_ = 0; j_ < 3; ++j_) {
            double s_ = t3_[i_][0] * m_->x[0][0][j_];
            for (int p_ = 1; p_ < 3; ++p_) s_ = s_ + t3_[i_][p_] * m_->x[0][p_][j_];
            t4_[i_][j_] = s_;
        }
]])
# An inverse's product may be NaN, and is tested by its name.
holds(compile_loops_nan solve.h
      [[        solve_inverse3_(t0_);
        double t1_[3][3];
        for (int i_ = 0; i_ < 3; ++i_)
            for (int j_ = 0; j_ < 3; ++j_) {
                double s_ = t0_[i_][0] * m_->x[1][0][j_];
                for (int p_ = 1; p_ < 3; ++p_) s_ = s_ + t0_[i_][p_] * m_->x[1][p_][j_];
                t1_[i_][j_] = s_;
            }
        const int t2_ = isnan(t1_[0][0]) || isnan(t1_[0][1]) || isnan(t1_[0][2]) || isnan(t1_[1][0]) || isnan(t1_[1][1]) || isnan(t1_[1][2]) || isnan(t1_[2][0]) || isnan(t1_[2][1]) || isnan(t1_[2][2]);
        m_->s[0] = isnan((t2_ ? NAN : t1_[0][0])) ? NAN : (t2_ ? NAN : t1_[0][0]) > 0.0 ? 1.0 : 0.0;
]])
# A power of a constant is NaN only where its exponent is, which names
# nothing: its product's cell is read untested, as written out.
like(compile_loops_raised raised.h [[    m_->y[0] = (t#_[0][1] > 0.0 ? 1.0 : 0.0);
]])
# The RIEKF's 12-state filter, 570,366 bytes at 9ed063c, 78,990 looped;
# the transformer's forward pass and U trained, 595,708 and 645,563,
# 390,104 and 433,781.
set(args --compile "${CMAKE_CURRENT_LIST_DIR}/compile/iekf.ink" riekf -o riekf.h)
check(compile_loops_riekf)
smaller(compile_loops_riekf riekf.h 90000)
foreach(model inf tune)
    set(args --compile ${transformer} ${model} -o ${model}.h)
    check(compile_loops_${model})
endforeach()
smaller(compile_loops_inf inf.h 430000)
smaller(compile_loops_tune tune.h 480000)

# A split step's scratch is a member of the model (DESIGN.md, a split step's
# scratch in the model struct): twins' two instances share their place, and
# tune's step holds its values no longer.
set(args --compile "${CMAKE_CURRENT_LIST_DIR}/compile/split.ink" twice -o twice.h)
check(compile_scratch)
file(READ "${OUT}/twice.h" twice)
holds(compile_scratch_shared twice.h [[
/* What a part of the step computes and a later part reads, held by the
 * model rather than the stack; instances stepped in turn share a place. */
typedef struct twice_a_held_values_ {
    double t2_[3][3];
    int t3_;
    double t5_;
    int t7_;
} twice_a_held_values_;

typedef struct twice_b_held_values_ {
    double t10_[3][3];
    int t11_;
    double t13_;
    int t15_;
} twice_b_held_values_;

typedef struct twice_values_ {
    union {
        twice_a_held_values_ a_held;
        twice_b_held_values_ b_held;
    } u_;
} twice_values_;

/* The parameters, which the host may assign]] [[
    } b_held;
    twice_values_ v_;
} twice;
]] "static TWICE_NOINLINE_ void twice_step1_(twice* m_, twice_a_held_values_* v_) {\n"
   "static TWICE_NOINLINE_ void twice_step18_(twice* m_, twice_b_held_values_* v_) {\n"
   "    twice_step1_(m_, &m_->v_.u_.a_held);\n" "    twice_step18_(m_, &m_->v_.u_.b_held);\n")
if(twice MATCHES "m_v_")
    message(SEND_ERROR "compile_scratch_shared: twice.h steps with values of its own")
endif()
set(args --compile "${CMAKE_CURRENT_LIST_DIR}/compile/split.ink" mine -o mine.h)
check(compile_scratch)
holds(compile_scratch_named mine.h "    } mine;\n    mine_values_ v_;\n} mine;\n")
strict(compile_scratch_named mine.h)
holds(compile_scratch_member tune.h [[
/* What a part of the step computes and a later part reads, held by the
 * model rather than the stack; instances stepped in turn share a place. */
typedef struct tune_values_ {
]] "    tune_values_ v_;\n} tune;\n")
# The step's own frame, its parts apart, by -fstack-usage, which GCC and
# Clang write: 12,568 bytes or more while it held tune's values.
if(CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
    file(WRITE "${OUT}/bound.c" "#include \"tune.h\"\nvoid stepped(tune* m, const double x[48], const double y[48]);\nvoid stepped(tune* m, const double x[48], const double y[48]) { tune_step(m, x, y); }\n")
    execute_process(COMMAND "${CC}" -std=c11 -O0 -fstack-usage -Wall -Wextra -Wpedantic
                            -Wconversion -Wshadow -Werror -c bound.c
                    WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE failed
                    ERROR_VARIABLE errors OUTPUT_VARIABLE errors)
    set(usage "")
    if(NOT failed)
        file(READ "${OUT}/bound.su" usage)
    endif()
    if(NOT usage MATCHES ":tune_step\t([0-9]+)\t" OR NOT CMAKE_MATCH_1 LESS 4096)
        message(SEND_ERROR "compile_scratch_stack: tune's step not under 4,096 bytes of "
                           "stack\n${errors}${usage}")
    endif()
endif()

# 2, the sweep's G*X', G's adjoints made an array, X' read in place.
like(compile_loops_sweep dense.h
     [[    double t#_[3][3] = {{t#_, t#_, t#_}, {t#_, t#_, t#_}, {t#_, t#_, t#_}};
    double t#_[3][3];
    for (int i_ = 0; i_ < 3; ++i_)
        for (int j_ = 0; j_ < 3; ++j_) {
            double s_ = t#_[i_][0] * m_->X[0][j_][0];
            for (int p_ = 1; p_ < 3; ++p_) s_ = s_ + t#_[i_][p_] * m_->X[0][j_][p_];
            t#_[i_][j_] = s_;
        }
]])
# Each term added where its condition holds, and where any did the
# array's second slice.
like(compile_loops_sweep gated.h
     [[    double t#_[3][3] = {{t#_, t#_, t#_}, {t#_, t#_, t#_}, {t#_, t#_, t#_}};
    int t#_[3][3] = {{(t#_ != 0), (t#_ != 0), (t#_ != 0)}, {(t#_ != 0), (t#_ != 0), (t#_ != 0)}, {(t#_ != 0), (t#_ != 0), (t#_ != 0)}};
    double t#_[2][3][3];
    for (int i_ = 0; i_ < 3; ++i_)
        for (int j_ = 0; j_ < 3; ++j_) {
            int f_ = t#_[i_][0];
            double s_ = f_ ? t#_[i_][0] * m_->X[0][j_][0] : 0.0;
            for (int p_ = 1; p_ < 3; ++p_) {
                const int c_ = t#_[i_][p_];
                s_ = c_ ? s_ + t#_[i_][p_] * m_->X[0][j_][p_] : s_;
                f_ = f_ || c_;
            }
            t#_[0][i_][j_] = s_;
            t#_[1][i_][j_] = f_ ? 1.0 : 0.0;
        }
]] [[    m_->W[0][0][1] = m_->index_ == 0 ? 0.0 : m_->W[1][0][1] + (0.0 - m_->eta * t#_[0][0][1]);
]])
# Some adjoints absent: written out, as today.
like(compile_loops_sweep diag.h
     [[    m_->W[0][0][1] = m_->index_ == 0 ? 0.0 : m_->W[1][0][1] + (0.0 - m_->eta * (isnan(t#_) ? NAN : t#_ * m_->X[0][1][0]));
]])
set(args --compile ${loops} gated --float -o float/gated.h)
check(compile_loops_float)
like(compile_loops_float float/gated.h [[    float t#_[2][3][3];
]] [[            t#_[1][i_][j_] = f_ ? 1.0f : 0.0f;
]])

# 3, a term of 9 cells or more each the first's at its place, and its
# whole-term NaN test; a term's cells otherwise, as today.
holds(compile_loops_terms spin.h [[    for (int i_ = 0; i_ < 3; ++i_)
        for (int j_ = 0; j_ < 3; ++j_) m_->y[0][i_][j_] = t2_[i_][j_];
]] [[    for (int i_ = 0; i_ < 3; ++i_)
        for (int j_ = 0; j_ < 3; ++j_) m_->z[0][i_][j_] = t4_[i_][j_];
]] [[    m_->w[0][1][2][2] = m_->index_ == 0 ? 1.0 : t0_[1][2][2];
]] [[    m_->x[0][2][2] = m_->index_ == 0 ? 9.0 : t1_[2][2];
]])
holds(compile_loops_terms solve.h [[    for (int i_ = 0; i_ < 3; ++i_)
        for (int j_ = 0; j_ < 3; ++j_) m_->x[0][i_][j_] = m_->index_ == 0 ? m_->x0[i_][j_] : t4_[i_][j_];
    for (int i_ = 0; i_ < 3; ++i_)
        for (int j_ = 0; j_ < 3; ++j_)
            if (isnan(m_->x[0][i_][j_])) m_->x[0][0][0] = NAN;
    if (isnan(m_->x[0][0][0]))
        for (int i_ = 0; i_ < 3; ++i_)
            for (int j_ = 0; j_ < 3; ++j_) m_->x[0][i_][j_] = NAN;
]])
like(compile_loops_terms dense.h [[    for (int i_ = 0; i_ < 3; ++i_)
        for (int j_ = 0; j_ < 3; ++j_) m_->W[0][i_][j_] = m_->index_ == 0 ? m_->W0[i_][j_] : m_->W[1][i_][j_] + (0.0 - m_->eta * (isnan(t#_) ? NAN : t#_[i_][j_]));
    for (int i_ = 0; i_ < 3; ++i_)
        for (int j_ = 0; j_ < 3; ++j_)
            if (isnan(m_->W[0][i_][j_])) m_->W[0][0][0] = NAN;
    if (isnan(m_->W[0][0][0]))
]])
# A line that calls a function, written out: a loop would call it at each
# cell.
like(compile_loops_terms turn.h [[    m_->x[0][0][1] = (t#_ ? NAN : m_->A[0][1]) * turn_cos(m_->u[0]) + (t#_ ? NAN : m_->B[0][1]) * turn_sin(m_->u[0]);
]])
foreach(file spin.h solve.h dense.h gated.h diag.h riekf.h)
    strict(compile_loops_strict ${file})
endforeach()
strict(compile_loops_strict float/gated.h -Wdouble-promotion)

if(PART EQUAL 2)
    return()
endif()
endif()

# Made in part 2 too, which parts 3 and 4 run without.
file(MAKE_DIRECTORY "${OUT}/float")

if(NOT PART OR PART EQUAL 3)

# grad refuses eig and smax where A moves (DESIGN.md, eig and smax). An eig
# where A moves compiles since two matrices compare, its header 1.7 MB, too
# slow a case for the sanitizers to write.
file(WRITE "${OUT}/eig.ink" "y_n = smax([n 2; 3 4])\ng_n = grad_(a = n) eig([a 1; 1 2])[1]\nh_n = grad_(a = n) smax([a 2; 3 4])\n")
set(args --compile eig.ink)
set(stdout "cannot compile g: grad cannot differentiate eig yet\ncannot compile h: grad cannot differentiate smax yet\n")
set(exit 1)
check(compile_eig_refused)

# C132: a gradient with respect to a matrix that nothing moves is a zero of
# the point's shape, not of the body's.
file(WRITE "${OUT}/c132.ink" "f(z)[i] | z[i] > 0 = 1\nf(z)[i] = 0\nu_n = grad_(v = [x_n; 1]) [1 1]*f(v)\n")
set(args --compile c132.ink -o c132.h)
check(compile_c132)
holds(compile_c132 c132.h "    m_->u[0][0][0] = 0.0;\n    m_->u[0][1][0] = 0.0;\n")

# C133: a comparison read as a value in a guard does not jump where its
# sides meet, as the interpreter's guard asks only for its value.
file(WRITE "${OUT}/c133.ink" "h(z) = 0\nh(z) | (z > 2)*z > -1 = z\nu_n = grad_(t = x_n) h(t)\n")
set(args --compile c133.ink -o c133.h)
check(compile_c133)
holds(compile_c133 c133.h "    m_->u[0] = (m_->x[0] > 2.0 ? 1.0 : 0.0) * m_->x[0] > -1.0 ? 1.0 : 0.0;\n")
# One in a function the guard calls jumps, as a call is not the guard.
file(WRITE "${OUT}/c133s.ink" "s(z) = (z > 2)*z\nk(z) = 0\nk(z) | s(z) > -1 = z\nw_n = grad_(t = x_n) k(t)\n")
set(args --compile c133s.ink -o c133s.h)
check(compile_c133s)
holds(compile_c133s c133s.h "    m_->w[0] = (isnan(t0_) ? NAN : isnan(t1_) ? NAN : t1_ > -1.0 ? 1.0 : 0.0);\n")

# C135: a constant is folded from its operands', which a call reading what
# moves may give.
file(WRITE "${OUT}/c135.ink" "h(z) = 3\nf(M) = h(M) + 1\nb_n = h(x_n) + 1\nc_n = grad_(t = x_n) (h(t) + 1)*t\nd_n = f([x_n; 1])\n")
set(args --compile c135.ink -o c135.h)
check(compile_c135)
holds(compile_c135 c135.h "    m_->b[0] = 4.0;\n" "    m_->c[0] = 4.0;\n" "    m_->d[0] = 4.0;\n")
# C193: so is a product whose left is a power, and A^-1*b solved from them.
file(WRITE "${OUT}/c193.ink" "h(z) = 3\nf(z) = 2^(1/2)*h(z)\nb_n = f(x_n)\nc_n = [2 0; 0 4]^-1*[h(x_n); 1]\n")
set(args --compile c193.ink -o c193.h)
check(compile_c193)
holds(compile_c193 c193.h "    m_->b[0] = 4.242640687119286;\n" "    m_->c[0][0][0] = 1.5;\n"
      "    m_->c[0][1][0] = 0.25;\n")

# C136: the interpreter refuses a matrix written whole before a cell's own
# clause, so a cell it refuses makes every cell NaN, by value and under grad.
file(WRITE "${OUT}/c136.ink" "k1(z) | z[1,1] > 0 = z\nk2(z) = k1(z)\nk2(z)[1,1] = 2\nw_n = k2([x_n])\n")
set(args --compile c136.ink -o c136.h)
check(compile_c136)
holds(compile_c136 c136.h "    m_->w[0] = (isnan(t0_) ? NAN : 2.0);\n")
file(WRITE "${OUT}/c136t.ink" "k1(z) | z[1,1] > 0 = z\ny_n = k1([x_n])\ny_n[1,1] = 2\n")
set(args --compile c136t.ink -o c136t.h)
check(compile_c136_term)
holds(compile_c136_term c136t.h "    m_->y[0] = (isnan(t0_) ? NAN : 2.0);\n")
file(WRITE "${OUT}/c136g.ink" "f1(z)[i,j] = z[i,j]\nf1(z)[1,1] | ((z[1,1])^2)^(1/2) >= 5 = 7\nf2(z) = f1(z)\nf2(z)[1,1] = 2\nu_n = grad_(t = x_n) f2([t])\n")
set(args --compile c136g.ink -o c136g.h)
check(compile_c136g)
holds(compile_c136g c136g.h "    const double t2_ = (isnan(t1_) ? NAN : 2.0);\n    m_->u[0] = (isnan(t2_) ? NAN : 0.0);\n")

# C115: a constant gradient at a point that moves folds with what reads it
# from its value, not by taking the point again (C135).
file(WRITE "${OUT}/c115.ink" "u_n = x_n - grad_(t = x_n) 3*t\n")
set(args --compile c115.ink -o c115.h)
check(compile_c115)
holds(compile_c115 c115.h "    m_->u[0] = m_->x[0] - 3.0;\n")

# C112: a global a call reads sees the globals, not the call's names.
file(WRITE "${OUT}/c112.ink" "f(x) = x + g\ng = x*2\nx = 5\ny_n = f(n)\n")
set(args --compile c112.ink -o c112.h)
check(compile_c112)
holds(compile_c112 c112.h "    m_->g = m_->x * 2.0;\n" "    m_->y[0] = (double)m_->index_ + m_->g;\n")
file(WRITE "${OUT}/c112m.ink" "m(x) = {\n    y = x + g\n}\ng = x*2\nx = 5\nz_n = m(n).y\n")
set(args --compile c112m.ink -o c112m.h)
check(compile_c112_instance)
holds(compile_c112_instance c112m.h "    m_->g = m_->x * 2.0;\n")

# C119: a term read in a call given the clause's index, give or take a
# constant, is read as the interpreter reads it.
file(WRITE "${OUT}/c119c.ink" "f(t) = x_t\ng(s) = f(s - 1)\ny_n = f(n)\nw_n = g(s = n)\n")
set(args --compile c119c.ink -o c119c.h)
check(compile_c119_call)
holds(compile_c119_call c119c.h "    m_->y[0] = m_->x[0];\n" "    m_->w[0] = m_->x[1];\n")

# Tensors compiled (DESIGN.md, compile/tensor.ink): a tensor kept as C keeps
# double O[B][T][D], and met slice by slice as interpreted.
set(tensor "${CMAKE_CURRENT_LIST_DIR}/compile/tensor.ink")
set(args --compile ${tensor} mha -o mha.h)
check(compile_tensor_mha)
holds(compile_tensor_mha mha.h [=[ *     mha_step(&m, X);  once for each index, the first 0
 *     m.O[0][b][i][j]  is then O_n, slice b+1, row i+1 and column j+1
 *
 * A step takes X_n (2x3x4), the input at its index. An input of more than one
 * cell is a pointer to its cells, row by row, slice after slice. After a step,
 * m.name[k] is name_(n-k) for each sequence: X and O. The parameters are
 * fields holding the model's defaults once mha_init has run: d = 2.0. After
 * assigning one, call mha_update. A term the interpreter would refuse is NaN,
]=] [=[typedef struct mha {
    double d;
    long long index_;
    double X[1][2][3][4];
    double O[1][2][3][4];
} mha;
]=] [=[MHA_API_ void mha_step(mha* m_, const double X_in_[24]) {
    ++m_->index_;
    memcpy(m_->X[0], X_in_, sizeof m_->X[0]);
]=])
set(args --compile ${tensor} ring -o ring.h)
check(compile_tensor_ring)
holds(compile_tensor_ring ring.h [=[/* Using it:
 *
 *     ring m;
 *     ring_init(&m);
 *     ring_step(&m, X);  once for each index, the first 0
 *     m.u[0][b][i][j]  is then u_n, slice b+1, row i+1 and column j+1
 *
 * A step takes X_n (2x1x2), the input at its index. An input of more than one
 * cell is a pointer to its cells, row by row, slice after slice. After a step,
 * m.name[k] is name_(n-k) for each sequence: X (k <= 1), d, s (k <= 1), c, e,
 * g, k, l, p, q and u. At another rate, m.name[k] is name_(m-k), m its latest
 * term's index: h, computed at the steps 2*m + 1. The parameters are fields
 * holding the model's defaults once ring_init has run: s0 (2x1x2). After
 * assigning one, call ring_update. A term the interpreter would refuse is NaN,
 * and so is every term that reads one, through a guard or a comparison as
 * through arithmetic. Built with -ffinite-math-only, which -ffast-math
 * implies, GCC removes the tests that make it so, and Clang warns of each NaN.
 */
]=] [=[typedef struct ring {
    double s0[2][1][2];
    long long index_;
    double X[2][2][1][2];
    double d[1][2][1][2];
    double s[2][2][1][2];
    double c[1];
    double e[1][2][1][2];
    double g[1][1][2];
    double h[1][2][1][2];
    double k[1][2][2][1];
    double l[1][2][1][2];
    double p[1][2][1][1];
    double q[1][2][1][2];
    double u[1][2][1][2];
} ring;
]=] [=[RING_API_ void ring_init(ring* m_) {
    memset(m_, 0, sizeof *m_);
    m_->s0[0][0][0] = 1.0;
    m_->s0[0][0][1] = 0.0;
    m_->s0[1][0][0] = 0.0;
    m_->s0[1][0][1] = 1.0;
    m_->X[0][0][0][0] = 1.0;
    m_->X[0][0][0][1] = 1.0;
    m_->X[0][1][0][1] = 1.0;
    m_->index_ = -1;
    ring_update(m_);
}
]=] [=[RING_API_ void ring_step(ring* m_, const double X_in_[4]) {
    ++m_->index_;
    memcpy(m_->X[1], m_->X[0], sizeof m_->X[1]);
    memcpy(m_->s[1], m_->s[0], sizeof m_->s[1]);
    memcpy(m_->X[0], X_in_, sizeof m_->X[0]);
]=] [=[    m_->s[0][0][0][0] = m_->index_ == 0 ? m_->s0[0][0][0] : m_->s[1][0][0][0] * 0.0 + m_->s[1][0][0][1] * -1.0 + m_->X[0][0][0][0];
    m_->s[0][0][0][1] = m_->index_ == 0 ? m_->s0[0][0][1] : m_->s[1][0][0][0] * 1.0 + m_->s[1][0][0][1] * 0.0 + m_->X[0][0][0][1];
    m_->s[0][1][0][0] = m_->index_ == 0 ? m_->s0[1][0][0] : m_->s[1][1][0][0] * 0.0 + m_->s[1][1][0][1] * -1.0 + m_->X[0][1][0][0];
    m_->s[0][1][0][1] = m_->index_ == 0 ? m_->s0[1][0][1] : m_->s[1][1][0][0] * 1.0 + m_->s[1][1][0][1] * 0.0 + m_->X[0][1][0][1];
    if (isnan(m_->s[0][0][0][0]) || isnan(m_->s[0][0][0][1]) || isnan(m_->s[0][1][0][0]) || isnan(m_->s[0][1][0][1]))
        for (int b_ = 0; b_ < 2; ++b_)
            for (int i_ = 0; i_ < 1; ++i_)
                for (int j_ = 0; j_ < 2; ++j_) m_->s[0][b_][i_][j_] = NAN;
]=])
set(args --compile ${tensor} ring --float -o float/ring.h)
check(compile_tensor_ring_float)
holds(compile_tensor_ring_float float/ring.h [=[typedef struct ring {
    float s0[2][1][2];
    long long index_;
    float X[2][2][1][2];
]=] [=[RING_API_ void ring_step(ring* m_, const float X_in_[4]) {
]=] [=[    m_->s[0][1][0][0] = m_->index_ == 0 ? m_->s0[1][0][0] : m_->s[1][1][0][0] * 0.0f + m_->s[1][1][0][1] * -1.0f + m_->X[0][1][0][0];
]=])
set(args --compile ${tensor} sgd -o sgd.h)
check(compile_tensor_sgd)
holds(compile_tensor_sgd sgd.h [=[typedef struct sgd {
    double eta;
    long long index_;
    double X[1][2][2][2];
    double Y[1][2][2][1];
    double w[2][2][1];
} sgd;
]=] [=[SGD_API_ void sgd_step(sgd* m_, const double X_in_[8], const double Y_in_[4]) {
]=])

# A sequence with parameters read at a constant index, compiled (DESIGN.md,
# compile/iterates.ink): each term below the one read a temporary, the one
# read written where it is read, a count read there compiled in, and what
# stays refused named in the interpreter's words where it has them.
set(iterates "${CMAKE_CURRENT_LIST_DIR}/compile/iterates.ink")
set(args --compile ${iterates} roots -o roots.h)
check(compile_iterates_roots)
holds(compile_iterates_roots roots.h [[    const double t0_ = sqrt(0.0 + m_->x[0]);
    const double t1_ = sqrt(0.0 + t0_);
]] [[    const double t126_ = sqrt(0.0 + t125_);
    const double t127_ = sqrt(0.0 + t126_);
    const double t128_ = t127_ * t127_;
]] [[    m_->h[0] = t254_ * t254_;
]])
set(args --compile ${iterates} mpc -o mpc.h)
check(compile_iterates_mpc)
holds(compile_iterates_mpc mpc.h " call mpc_update. Compiled in as constants, these cannot change: iters.\n")
file(WRITE "${OUT}/iterates.ink" "c_n = q(x_n)_3\nd_n = z(x_n)_2\ne_n = p(x_n)_2\nf_n = w(x_n)_70\ng_n = r(x_n)_(1/2)\nh_n = lim nw(x_n)\nnw(a)_0 = a\nnw(a)_k = nw(a)_(k-1)/2 + r(a)_2\np(x)_0 = x\np(x)_k = p(x)_(k+1)/2\nq(x)_0 = x\nq(x)_k = q(x)_(k-2) + 1\nr(x)_0 = x\nr(x)_k = r(x)_(k-1)/2 + 1\nw(x)_0 = x\nw(x)_k = w(x/2)_(k-1)\nz(x)_0 = x\nz(x)_k = z(x)_k/2\n")
set(args --compile iterates.ink)
set(stdout "cannot compile c: q has no clause for index -1
cannot compile d: z is defined by itself
cannot compile e: nested 64 deep, which a recursion its guards do not end would pass
cannot compile f: nested 64 deep, which a recursion its guards do not end would pass
cannot compile g: an index must be a whole number, not 0.5
")
set(exit 1)
check(compile_iterates_refused)

# C352: s(x) read through g as s(2*x) is another sequence, not lim s's own
# term, which the interpreter makes 3 and the step made 7.6e-11.
file(WRITE "${OUT}/rebound.ink" "g(x, j) = s(x)_(j-1)\ns(x)_0 = x\ns(x)_k = g(2*x, k)/2 + 1/2^k\ny_n = lim s(x_n)\n")
set(args --compile rebound.ink)
set(stdout "cannot compile y: s's term read with other arguments than its own\n")
set(exit 1)
check(compile_limit_rebound)

# Recursion from the tip (DESIGN.md, compile/tip.ink): the arm compiled, its
# backward pass a sequence read ahead to a guard at the tip; its values at 1
# and 5 Euler-Lagrange's, worked by sympy; and the read-aheads that stay
# refused, in the words of the same recursion written as a call.
set(tip "${CMAKE_CURRENT_LIST_DIR}/compile/tip.ink")
set(args --compile ${tip} arm -o tip.h)
check(compile_tip)
file(COPY ${tip} DESTINATION "${OUT}")
file(WRITE "${OUT}/values.ink" [[
>> use tip
use tip

>> tip.reach.y_1 == [61421137/1599000; 11300867/1199250; 146362649/23985000]
1

>> tip.reach.y_5 == [441421/23400; 427261/72150; -151031/57720]
1
]])
set(args --check values.ink)
set(stdout "values.ink: 3 answers, each as recorded\n")
check(compile_tip_values)
# a reads past the tip, b's guard does not fold, c has no tip, d reads its own
# stream ahead; e, with a base, compiles as its function form f does.
file(WRITE "${OUT}/ahead.ink" "a_n = up(x_n)_5\nb_n = w(x_n)_1\nc_n = p(x_n)_2\nd_n = d_(n+1)\n"
     "e_n = h(x_n)_10\nf_n = hf(x_n, 10)\nup(x)_k | k == 3 = x\nup(x)_k = up(x)_(k+1)/2\n"
     "w(x)_k | x > k = x\nw(x)_k = w(x)_(k+1)/2\np(x)_0 = x\np(x)_k = p(x)_(k+1)/2\nh(x)_0 = x\n"
     "h(x)_k | k == 70 = x\nh(x)_k = h(x)_(k+1)/2 + x\nhf(x, k) | k == 0 = x\n"
     "hf(x, k) | k == 70 = x\nhf(x, k) = hf(x, k+1)/2 + x\n")
set(args --compile ahead.ink)
set(stdout "cannot compile a: nested 64 deep, which a recursion its guards do not end would pass
cannot compile b: nested 64 deep, which a recursion its guards do not end would pass
cannot compile c: nested 64 deep, which a recursion its guards do not end would pass
cannot compile d: d_(...): a term after the one being computed
")
set(exit 1)
check(compile_tip_refused)
# C363: a halving recursion with a base filled each term below at every read
# of it, 96 s to compile a, read ahead, and over 60 s for b, refused anyway.
# A measure of time, so in an optimised build only: 16 s in Debug, past 180
# under the sanitizers.
if(TIMED)
file(WRITE "${OUT}/halving.ink" "a_n = v(x_n)_999\nb_n = w(x_n)_1000\nv(x)_0 = x\nv(x)_k | k > 999 = x\n"
     "v(x)_k = v(x/2)_(k+1)\nw(x)_0 = x\nw(x)_k = w(x/2)_(k-1)\n")
set(args --compile halving.ink)
set(stdout "cannot compile b: nested 64 deep, which a recursion its guards do not end would pass\n")
set(exit 1)
check(compile_c363)
endif()

# A sequence with parameters read at lim's walking index (DESIGN.md,
# compile/series.ink): the series compiled; their values, e^x's at -2 and 1,
# Rodrigues' at n = 0 and 20 and the sine's at -5 and 1/2, by mpmath, each
# held within 1e-9 as a limit stops within about 1e-10, and answered
# inexactly; and what stays refused.
set(series "${CMAKE_CURRENT_LIST_DIR}/compile/series.ink")
set(args --compile ${series} -o series.h)
check(compile_series)
file(COPY ${series} DESTINATION "${OUT}")
file(WRITE "${OUT}/walked.ink" [[
>> use series
use series

>> far(A, B) = sum_(j=1)^3 sum_(k=1)^3 abs(A[j,k] - B[j,k])
far(A, B) = sum_(j=1)^3 sum_(k=1)^3 abs(A[j,k] - B[j,k])

>> abs(series.taylor.y_0 - 0.1353352832366126918939995) < 10^-9
~1

>> abs(series.taylor.y_30 - 2.718281828459045235360287) < 10^-9
~1

>> far(series.taylor.R_0, [0.91444306659383029383, -0.24282873813667482718, -0.32377165084889976958; 0.24282873813667482718, 0.96919950397377890578, -0.041067328034961458963; 0.32377165084889976958, -0.041067328034961458963, 0.94524356262005138805]) < 10^-9
~1

>> far(series.taylor.R_20, [0.92135885972975595941, -0.35488114287926168634, -0.15861029609137275277; 0.052899164241524570488, 0.51871622154610647161, -0.85330836157128965314; 0.38509678006967558967, 0.77781286691185537418, 0.49669670227043814024]) < 10^-9
~1

>> abs(series.wave.z_0 - 0.9589242746631384688931544) < 10^-9
~1

>> abs(series.wave.z_55 - 0.4794255386042030002732879) < 10^-9
~1
]])
set(args --check walked.ink)
set(stdout "walked.ink: 8 answers, each as recorded\n")
check(compile_series_values)

# A sequence with parameters walked (DESIGN.md): the values of
# compile/seqcells.ink's instances, by fractions, and by the eigenvalues'
# closed forms at 40 digits, rho and abscissa held within 1e-14, the
# interpreter's bisection within a unit or two of a double of them; which
# --check holds the walk to, in part 5.
file(COPY "${CMAKE_CURRENT_LIST_DIR}/compile/seqcells.ink" DESTINATION "${OUT}")
file(WRITE "${OUT}/seqvalues.ink" [[
>> use seqcells
use seqcells

>> seqcells.heat.a_7 == [13/64; 45/128; 69/128; 21/32; 21/32; 69/128; 45/128; 13/64]
~1

>> abs(seqcells.rim.e_5 - 0.6222098214285714) < 10^-15
~1

>> seqcells.sens.g_5 == 0.6005859375
1

>> seqcells.sens.g_7 == 0.5572509765625
1

>> seqcells.sens.v_9 == [1/64; 3/32; 15/64; 5/16; 15/64; 3/32; 1/64; 0]
1

>> seqcells.idx.a_5 == 1.95703125
~1

>> frac seqcells.idx.e_3
43/24

>> abs(seqcells.pair.r_0 - 1.62838821814150114735) < 10^-14
~1

>> abs(seqcells.pair.a_0 + 1.0716117818584989192) < 10^-14
~1

>> abs(seqcells.pair.r_3 - 0.55226805085936302664) < 10^-14
~1

>> abs(seqcells.pair.a_3 + 0.4499999999999999833) < 10^-14
~1

>> abs(seqcells.pair.r_8 - 1.05118980208143185527) < 10^-14
~1

>> abs(seqcells.pair.a_8 - 1.04999999999999993338) < 10^-14
~1

>> [seqcells.pair.h_4, seqcells.pair.h_5, seqcells.pair.s_1, seqcells.pair.s_2, seqcells.pair.s_7, seqcells.pair.s_8] == [1, 0, 0, 1, 1, 0]
~1

>> abs(seqcells.quad.r_1 - 1.21106014163899665394) < 10^-14
~1

>> abs(seqcells.quad.a_1 - 0.68333333333333331112) < 10^-14
~1

>> abs(seqcells.quad.r_5 - 0.96609178307929590299) < 10^-14
~1

>> abs(seqcells.quad.a_5 - 0.28333333333333333055) < 10^-14
~1

>> abs(seqcells.quad.r_9 - 1.62838821814150114735) < 10^-14
~1

>> abs(seqcells.quad.a_9 - 1.62838821814150114735) < 10^-14
~1

>> [seqcells.quad.h_6, seqcells.quad.s_4, seqcells.quad.s_5, seqcells.quad.s_7, seqcells.quad.s_8] == [0, 0, 1, 1, 0]
~1

>> abs(seqcells.forms.a_5[1] - 315/224) + abs(seqcells.forms.a_5[2] - 2) < 10^-15
~1

>> abs(seqcells.forms.b_4 - (4/7 + 2)) < 10^-15
~1

>> abs(seqcells.forms.c_3 - 259/448) < 10^-15
~1

>> abs(seqcells.forms.d_6 - (6/7/64 + 6)) < 10^-15
~1

>> abs(seqcells.fixed.y_5[1] - 10/7) + abs(seqcells.fixed.y_5[2] - 4) < 10^-9
~1

>> seqcells.degen.r_5 == ~(1/3)
~1
]])
set(args --check seqvalues.ink)
set(stdout "seqvalues.ink: 28 answers, each as recorded\n")
check(compile_seqcells_values)

# Definitions by cells as loops (DESIGN.md, next in line): the values of
# compile/cellloops.ink's instances, by Python's fractions, and by its
# doubles in the interpreter's order, the objective summed by rows and in
# one sum 1.6e-12 apart; which --check then holds the loops to, once built.
file(COPY "${CMAKE_CURRENT_LIST_DIR}/compile/cellloops.ink" DESTINATION "${OUT}")
file(WRITE "${OUT}/cellvalues.ink" [=[
>> use cellloops
use cellloops

>> [cellloops.exact.objective_0, cellloops.exact.objective_3]
[217, 217]

>> [cellloops.held.field_0[1, 1], cellloops.held.field_1[1, 1], cellloops.held.field_2[1, 1], cellloops.held.field_3[1, 1]] == [5/8, 11/16, 13/16, 7/8]
1

>> [cellloops.held.field_0[32, 32], cellloops.held.field_1[32, 32], cellloops.held.field_2[32, 32], cellloops.held.field_3[32, 32]] == [-3/4, 5/16, 7/16, 1/2]
1

>> cellloops.seen.objective_0 == ~241.35873724489798
~1

>> cellloops.flat.objective_0 == ~241.35873724489957
~1

>> cellloops.tanked.objective_0 == ~976.5610650510209
~1

>> cellloops.tankflat.objective_0 == ~976.5610650510386
~1

>> [cellloops.rod.s_0, cellloops.rod.s_5] == [384, 384]
1

>> [cellloops.rod.x_0[1], cellloops.rod.x_1[1], cellloops.rod.x_2[1], cellloops.rod.x_0[1024], cellloops.rod.x_1[1024], cellloops.rod.x_2[1024]] == [5/16, 9/16, 9/16, 3/16, 3/16, 7/16]
1

>> [cellloops.ends.t_0, cellloops.ends.t_1, cellloops.ends.t_2, cellloops.ends.w_0, cellloops.ends.w_1, cellloops.ends.w_2] == [1501/4, 751/2, 1503/4, 1512, 1505, 1498]
1

>> [cellloops.outs.s_0, cellloops.outs.v_0, cellloops.outs.v_1, cellloops.outs.v_2] == [750, 1503/4, 1505/4, 1503/4]
1

>> [cellloops.outs.u_0, cellloops.outs.u_1, cellloops.outs.u_2, cellloops.outs.z_0, cellloops.outs.z_1, cellloops.outs.z_2] == [377, 1497/4, 753/2, 1755/4, 1253/4, 1751/4]
1

>> [cellloops.gate.q_0, cellloops.gate.q_4, cellloops.pulled.d_3] == [192, 192, 3/4]
1

>> [cellloops.gate.d_0[1], cellloops.gate.d_0[1024]] == [1/4, -1023]
1
]=])
set(args --check cellvalues.ink)
set(stdout "cellvalues.ink: 15 answers, each as recorded\n")
check(compile_cellloops_values)
# a and b read t elsewhere than at the walking index less a constant, which
# compile, t walked at each iterate; c and d give it arguments that change as
# the walk does; e differentiates through it;
# f's term reads the limit's own at its index, and g's its own with other
# arguments; before the walk, j's term reads the limit's own, and so does
# h's guard below its base.
file(WRITE "${OUT}/walking.ink" "a_n = lim sa(x_n)\nb_n = lim sb(x_n)\nc_n = lim sc(x_n)\n"
     "d_n = lim sd(x_n)\ne_n = lim se(x_n)\nf_n = lim sf(x_n)\ng_n = lim sg(x_n)\n"
     "h_n = lim sh(x_n)\nq(x)_3 = x\nq(x)_k | k < 3 = sh(x)_(k-1)/2\nq(x)_k = q(x)_(k-1)*x/k\n"
     "sh(x)_0 = 1\nsh(x)_1 = 1\nsh(x)_k = sh(x)_(k-1)/2 + q(x)_(k-1)/4\n"
     "j_n = lim sj(x_n)\nr(x)_k = sj(x)_(k-1)/4 + t(x)_k\nsj(x)_0 = 1\nsj(x)_1 = 1\n"
     "sj(x)_k = sj(x)_(k-1)/2 + r(x)_(k-1)/2\n"
     "t(x)_0 = 1\nt(x)_k = t(x)_(k-1)*x/k\nu(x)_k = sf(x)_k/2\nw(x)_0 = x\n"
     "w(x)_k = w(x/2)_(k-1)/k\nsa(x)_0 = 1\nsa(x)_k = sa(x)_(k-1) + t(x)_(2*k)\nsb(x)_0 = 1\n"
     "sb(x)_k = sb(x)_(k-1) + t(x)_(k+1)\nsc(x)_0 = 1\n"
     "sc(x)_k = sc(x)_(k-1) + t(sc(x)_(k-1))_k\nsd(x)_0 = 1\nsd(x)_k = sd(x)_(k-1) + t(k*x)_k\n"
     "se(x)_0 = 1\nse(x)_k = se(x)_(k-1) + grad_(y = x) t(y)_k\nsf(x)_0 = 1\n"
     "sf(x)_k = sf(x)_(k-1)/2 + u(x)_(k-1)\nsg(x)_0 = 0\nsg(x)_k = sg(x)_(k-1) + w(x)_k\n")
set(args --compile walking.ink)
set(stdout "cannot compile c: t_(...): read at lim sc's index with arguments that change as it walks
cannot compile d: t_(...): read at lim sd's index with arguments that change as it walks
cannot compile e: t_(...): a derivative of a term lim se walks, for now
cannot compile f: u_(...): reads lim sf's term at its index
cannot compile g: w_(...): read at lim sg's index with other arguments in its own terms
cannot compile h: q_(...): its terms before the walk read lim sh's own
cannot compile j: r_(...): its terms before the walk read lim sj's own
")
set(exit 1)
check(compile_series_refused)

# An unbounded sum compiled (DESIGN.md, compile/sums.ink): the file
# compiled; the series' values by closed forms, e^x's at -2 and 1, the sine's
# at -2 and 7/2, the product's 1/(1 - y^2) at y = -1/5 and 3/10, and
# Rodrigues' formula and the left Jacobian at n = 0 and 20, by mpmath, each
# held within 1e-9 as a limit stops within about 1e-10; the factorial's,
# exact or refused; a sum's walk as long as the interpreter's; and what
# stays refused.
set(sums "${CMAKE_CURRENT_LIST_DIR}/compile/sums.ink")
set(args --compile ${sums} -o sums.h)
check(compile_sums)
# e^x's !q walked, the one before times q; the sine's !(2*q + 1) the helper's.
file(READ "${OUT}/sums.h" text)
string(FIND "${text}" "sums_fact_((double)k_)" at)
if(NOT at EQUAL -1)
    message(SEND_ERROR "compile_sums: sums.h calls the factorial for e^x's !q")
endif()
holds(compile_sums sums.h "sums_fact_(2.0 * (double)k_ + 1.0)")
file(COPY ${sums} DESTINATION "${OUT}")
file(WRITE "${OUT}/summed.ink" [[
>> use sums
use sums

>> far(A, B) = sum_(j=1)^3 sum_(k=1)^3 abs(A[j,k] - B[j,k])
far(A, B) = sum_(j=1)^3 sum_(k=1)^3 abs(A[j,k] - B[j,k])

>> abs(sums.summed.y_0 - 0.1353352832366126918939995) < 10^-9
~1

>> abs(sums.summed.y_30 - 2.718281828459045235360287) < 10^-9
~1

>> abs(sums.summed.z_0 + 0.9092974268256816953960199) < 10^-9
~1

>> abs(sums.summed.z_55 + 0.3507832276896198481203688) < 10^-9
~1

>> abs(sums.summed.u_30 - 2.718281828459045235360287) < 10^-9
~1

>> abs(sums.summed.p_0 - 1.041666666666666666666667) < 10^-9
~1

>> abs(sums.summed.p_50 - 1.098901098901098901098901) < 10^-9
~1

>> far(sums.summed.R_0, [0.91444306659383029383, -0.24282873813667482718, -0.32377165084889976958; 0.24282873813667482718, 0.96919950397377890578, -0.041067328034961458963; 0.32377165084889976958, -0.041067328034961458963, 0.94524356262005138805]) < 10^-9
~1

>> far(sums.summed.R_20, [0.92135885972975595941, -0.35488114287926168634, -0.15861029609137275277; 0.052899164241524570488, 0.51871622154610647161, -0.85330836157128965314; 0.38509678006967558967, 0.77781286691185537418, 0.49669670227043814024]) < 10^-9
~1

>> far(sums.summed.J_0, [0.97131495254669930874, -0.12320198410488437689, -0.16426931213984583585; 0.12320198410488437689, 0.98967338291681175115, -0.013768822777584331806; 0.16426931213984583585, -0.013768822777584331806, 0.98164156962988755759]) < 10^-9
~1

>> far(sums.summed.J_20, [0.97271606719549889255, -0.16562839297379354474, -0.1117021260803869632; 0.060858091004509292148, 0.83302233123645322243, -0.46606925570276620536; 0.19027985255735015265, 0.43987668021044514221, 0.82538283005119291234]) < 10^-9
~1

>> sums.halving.f_96
39916800

>> sums.halving.f_9
error: a factorial needs a whole number, not 0.125

>> sums.halving.f_0
error: a factorial cannot be negative
]])
set(args --check summed.ink)
set(stdout "summed.ink: 16 answers, each as recorded\n")
check(compile_sums_values)
# 100 partial sums at most, from 3 to 102, as the interpreter's.
file(WRITE "${OUT}/bound.ink" "y_n = sum_(q=3) x_n^q/!q\n")
set(args --compile bound.ink -o bound.h)
check(compile_sums_bound)
holds(compile_sums_bound bound.h "    for (long long k_ = 4; k_ <= 102; ++k_) {\n")
# A sum folded calls no factorial, so none is written: a check program's
# unused function is an error to Clang's -Werror.
file(WRITE "${OUT}/folded.ink" "ee(x) = sum_(q=0) x^q/!q\ny_n = ee(1) + x_n\n")
set(args --compile folded.ink -o folded.h)
check(compile_sums_folded)
file(READ "${OUT}/folded.h" text)
string(FIND "${text}" "_fact_" at)
if(NOT at EQUAL -1)
    message(SEND_ERROR "compile_sums_folded: folded.h writes a factorial nothing calls")
endif()
# C355: nor does a limit folded call an inverse, or the limit within it.
file(WRITE "${OUT}/unwalked.ink" "q(x)_0 = x\nq(x)_k = q(x)_(k-1)/2 + 1\ns(x)_0 = x\n"
     "s(x)_k = s(x)_(k-1)/2 + lim q(x)/2^k\np(a)_0 = [0, 0; 0, 0]\n"
     "p(a)_k = p(a)_(k-1)/2 + (a*[1, 0; 0, 1] + k*[1, 0; 0, 1])^(-1)/2^k\n"
     "y_n = (lim p(1))[1,1] + lim s(1) + x_n\n")
foreach(floats "" --float)
    set(args --compile unwalked.ink ${floats} -o unwalked.h)
    check(compile_limits_folded${floats})
    file(READ "${OUT}/unwalked.h" text)
    foreach(helper _inverse2_ _lim0 float.h)
        string(FIND "${text}" "${helper}" at)
        if(NOT at EQUAL -1)
            message(SEND_ERROR "compile_limits_folded${floats}: unwalked.h writes ${helper}")
        endif()
    endforeach()
endforeach()
# a starts where the step is; b's power is at twice the index, and f's
# inner bound moves; c gives t arguments that change as the walk does, and e,
# which compiles, reads it ahead, walked at each iterate; d differentiates
# through it; h calls an instance's own
# function at the index.
file(WRITE "${OUT}/unsummed.ink" "A = [0, -1; 1, 0]/2\nt(x)_0 = 1\nt(x)_k = t(x)_(k-1)*x/k\n"
     "mm(a = 1) = {\n    g(j) = a^j/!j\n    y = sum_(q=0) g(q)\n}\na_n = sum_(q=n) 2^-q\n"
     "b_n = sum_(q=0) (x_n*A)^(2*q)/!(2*q)\nc_n = sum_(q=0) t(q*x_n)_q\n"
     "d_n = grad_(y = x_n) sum_(q=0) y^q/!q\ne_n = sum_(q=0) t(x_n)_(q+1)\n"
     "f_n = sum_(q=0) sum_(j=0)^q x_n^j/!q\nh_n = mm(a = x_n).y\n")
set(args --compile unsummed.ink)
set(bounds ": a sum or a product whose bounds are not constants\n")
set(stdout "cannot compile a${bounds}cannot compile b: a matrix power whose exponent is not a constant
cannot compile c: t_(...): read at sum_(q=0)'s index with arguments that change as it walks
cannot compile d: a derivative of a power whose exponent is not a constant, for now
cannot compile f${bounds}cannot compile h: g, a function of an instance, called at sum_(q=0)'s index, for now
")
set(exit 1)
check(compile_sums_refused)

# Part 1's running totals checked, each instance to its recurrence's report
# (DESIGN.md, a cumulative sum compiled as an accumulator).
set(story "${CMAKE_CURRENT_LIST_DIR}/compile/accum.ink")
if(CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
    # An instance's report, built uncontracted as test/CMakeLists.txt builds a
    # check (C97), and its exit.
    function(reported name instance steps)
        set(args --check ${story} ${instance} --steps ${steps} ${ARGN} -o ${name}.c)
        check(check_accum_${name})
        execute_process(COMMAND "${CC}" -std=c11 -O2 -ffp-contract=off ${name}.c -o ${name} -lm
                        WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE failed
                        ERROR_VARIABLE errors OUTPUT_VARIABLE errors)
        if(failed)
            message(SEND_ERROR "check_accum_${name}: ${name}.c does not build\n${errors}")
        endif()
        execute_process(COMMAND "${OUT}/${name}" WORKING_DIRECTORY "${OUT}"
                        OUTPUT_VARIABLE report RESULT_VARIABLE code TIMEOUT ${CASE_TIMEOUT})
        set(report_${name} "${report}" PARENT_SCOPE)
        set(code_${name} "${code}" PARENT_SCOPE)
    endfunction()
    # The instance's report is its recurrence's but for the name and run's lines.
    function(compared name instance steps)
        reported(${name} ${instance} ${steps} ${ARGN})
        reported(rec${name} rec${instance} ${steps} ${ARGN})
        string(REPLACE "rec${instance}" "${instance}" twin "${report_rec${name}}")
        string(REGEX REPLACE "\n${instance}\\.run:[^\n]*" "" twin "${twin}")
        if(NOT code_${name} EQUAL 0 OR NOT code_rec${name} EQUAL 0 OR
           NOT "${report_${name}}" STREQUAL "${twin}" OR
           NOT "${twin}" MATCHES "^${instance}: ${steps} steps from 0")
            message(SEND_ERROR "check_accum_${name}: exit ${code_${name}}\n${report_${name}}--- where rec${instance}'s, exit ${code_rec${name}}, is\n${report_rec${name}}")
        endif()
        set(report_${name} "${report_${name}}" PARENT_SCOPE)
    endfunction()
    foreach(instance IN ITEMS m ag sv ls ev dv mm ey hy fy ms)
        compared(${instance} ${instance} 100)
    endforeach()
    compared(ol ol 300)
    compared(olf ol 300 --float)
    # By Python: the header's operations in doubles against fractions.
    foreach(line "fs: within 0" "x: within 1\\.1e-16" "f: within 5\\.6e-17" "R: within 3\\.6e-14")
        if(NOT "${report_ol}" MATCHES "\nol\\.${line}\n")
            message(SEND_ERROR "check_accum_ol: no line ol.${line} in\n${report_ol}")
        endif()
    endforeach()
endif()

if(PART EQUAL 3)
    return()
endif()
endif()

if(NOT PART OR PART EQUAL 4)

# A frequency response does not compile: a compiled value is real, so the
# complex e^(i*w*n) is refused, not its abs.
file(WRITE "${OUT}/response.ink" "y_n = abs(e^(i*w*n))\n")
set(args --compile response.ink)
set(stdout "cannot compile y: a complex number\n")
set(exit 1)
check(compile_response_refused)

# Block literals compiled (DESIGN.md, compile/blocks.ink): the header the
# same literals written cell by cell give, each block's cells read where
# they are, and what stays refused.
set(plant "A = [1, 1; 0, 1]\nB = [0; 1]\nC = [1, 0]\nK = [3, 3]\nki = 1\n")
set(loop "x_0 = [0; 0; 0]\nx_n = (Aa - Ba*Kf)*x_(n-1) + [0; 0; r_n]\n")
file(WRITE "${OUT}/blocks/aug.ink" "${plant}Aa = [A, 0; -C, 1]\nBa = [B; 0]\nKf = [K, -ki]\n${loop}y_n = [C, 0]*x_n\n")
file(WRITE "${OUT}/cells/aug.ink" "${plant}Aa = [A[1,1], A[1,2], 0; A[2,1], A[2,2], 0; -C[1,1], -C[1,2], 1]\nBa = [B[1,1]; B[2,1]; 0]\nKf = [K[1,1], K[1,2], -ki]\n${loop}y_n = [C[1,1], C[1,2], 0]*x_n\n")
foreach(form blocks cells)
    set(args --compile ${form}/aug.ink -o ${form}/aug.h)
    check(compile_blocks_${form})
endforeach()
file(READ "${OUT}/blocks/aug.h" by_blocks)
file(READ "${OUT}/cells/aug.h" by_cells)
if(NOT by_blocks STREQUAL by_cells)
    message(SEND_ERROR "compile_blocks: aug.h by blocks is not aug.h by cells")
endif()
holds(compile_blocks_blocks blocks/aug.h [[    m_->x[0][2][0] = m_->index_ == 0 ? 0.0 : (0.0 - m_->C[0][0] + (0.0 - 0.0 * m_->K[0][0])) * m_->x[1][0][0] + (0.0 - m_->C[0][1] + (0.0 - 0.0 * m_->K[0][1])) * m_->x[1][1][0] + (1.0 + (0.0 - 0.0 * (0.0 - m_->ki))) * m_->x[1][2][0] + m_->r[0];
]] [[    m_->y[0] = m_->C[0][0] * m_->x[0][0][0] + m_->C[0][1] * m_->x[0][1][0] + 0.0 * m_->x[0][2][0];
]])
file(WRITE "${OUT}/blocks.ink" "a_n = [T, x_n]\nb_n = [A, [x_n, 1]]\nc_0 = x_0\nc_n = [c_(n-1), x_n]\nd_n = lim e(x_n)\ne(r)_0 = [I, I]\ne(r)_k = [(I + r*f(e(r)_(k-1)))^(0-1), I]\nf(S)[j<=2, k<=2] = S[j, k]\nA = [1, 2; 3, 4]\nI[j<=2, k<=2] = j == k\nT = [1;; 2]\nx_n = n\n")
set(args --compile blocks.ink)
set(stdout "cannot compile a: a tensor cannot be a block of a literal, only a matrix can
cannot compile b: a block that does not fill its band
cannot compile c: its clauses have different shapes
")
set(exit 1)
check(compile_blocks_refused)

# C220: a value derived from a parameter whose cell is a temporary of the
# update, a stretched value or an inverse's, is read from its field, not
# by the temporary's name, which the step does not declare.
file(WRITE "${OUT}/c220.ink" "p = 1/2\nQ = [exp(p), [1, 2; 3, 4]]\nR = [p, 1; 2, 3]^(0-1)\nx_n = Q*[n; 1; 1] + R*[n; 1]\n")
set(args --compile c220.ink -o c220.h)
check(compile_c220)
holds(compile_c220 c220.h [[    m_->x[0][0][0] = m_->Q[0][0] * (double)m_->index_ + 1.0 * 1.0 + 2.0 * 1.0 + (m_->R[0][0] * (double)m_->index_ + m_->R[0][1] * 1.0);
]])

# Temporaries in a limit's function (DESIGN.md, compile/implicit.ink): each
# value of Robertson's Newton iterate written once, h read 12 times, by
# Cramer's rule, through grad's Jacobian, and by the inverse called once.
set(rober "k1 = 0.04\nk2 = 3*10^7\nk3 = 10^4\nf(y) = [-k1*y[1] + k3*y[2]*y[3]; k1*y[1] - k3*y[2]*y[3] - k2*y[2]*y[2]; k2*y[2]*y[2]]\nJ(y) = [-k1, k3*y[3], k3*y[2]; k1, -k3*y[3] - 2*k2*y[2], -k3*y[2]; 0, 2*k2*y[2], 0]\nJg(y)[j<=3, k<=3] = (grad_(v = y) f(v)[j])[k]\nI = [1 0 0; 0 1 0; 0 0 1]\nc(j) = mod(j - 1, 3) + 1\nadj(A)[j<=3, k<=3] = A[c(k+1), c(j+1)]*A[c(k+2), c(j+2)] - A[c(k+1), c(j+2)]*A[c(k+2), c(j+1)]\ndet3(A) = (A[1]*adj(A))[1,1]\nsolve(A, b) = adj(A)*b/det3(A)\nh = 1/10\nnw(yp)_0 = yp\ny_0 = [1; 0; 0]\ny_n = lim nw(y_(n-1))\nnw(yp)_k = nw(yp)_(k-1) - ")
set(newton "nw(yp)_(k-1) - yp - h*f(nw(yp)_(k-1))")
file(WRITE "${OUT}/solve/cramer.ink" "${rober}solve(I - h*J(nw(yp)_(k-1)), ${newton})\n")
file(WRITE "${OUT}/grad/cramer.ink" "${rober}solve(I - h*Jg(nw(yp)_(k-1)), ${newton})\n")
file(WRITE "${OUT}/inverse/cramer.ink" "${rober}(I - h*J(nw(yp)_(k-1)))^-1*(${newton})\n")
foreach(form solve grad inverse)
    set(args --compile ${form}/cramer.ink -o ${form}/cramer.h)
    check(compile_implicit_${form})
    file(READ "${OUT}/${form}/cramer.h" text)
    string(REGEX MATCHALL "m_->h \\*" reads "${text}")
    list(LENGTH reads reads)
    file(SIZE "${OUT}/${form}/cramer.h" size)
    if(NOT reads EQUAL 12 OR NOT size LESS 32768)
        message(SEND_ERROR "compile_implicit_${form}: h read ${reads} times in ${size} bytes")
    endif()
endforeach()
string(REGEX MATCHALL "cramer_inverse3_\\(" calls "${text}")
list(LENGTH calls calls)
if(NOT calls EQUAL 2)
    message(SEND_ERROR "compile_implicit_inverse: cramer_inverse3_ written ${calls} times, not 2")
endif()
set(args --compile inverse/cramer.ink --float -o float/cramer.h)
check(compile_float_implicit)
file(READ "${OUT}/float/cramer.h" text)
string(FIND "${text}" "double" at)
if(NOT at EQUAL -1)
    message(SEND_ERROR "compile_float_implicit: float/cramer.h holds 'double'")
endif()
holds(compile_float_implicit float/cramer.h "float v1_[3][3] = ")

# A limit's invariants hoisted (DESIGN.md): layered's inner limit computed
# once, before its walk's loop, and so a limit of numbers and a factorial
# that read only the arguments, but not a limit a base clause reads.
set(args --compile ${CMAKE_CURRENT_LIST_DIR}/compile/implicit.ink layers -o layered.h)
check(compile_hoisted)
file(WRITE "${OUT}/scalar.ink" "layers(x_n) = {\n    s(v)_0 = v\n    s(v)_k = s(v)_(k-1)/2 + v\n"
     "    o(a)_0 = a\n    o(a)_k = o(a)_(k-1)/4 + lim s(a + 1)\n    q_n = lim o(x_n)\n}\n")
set(args --compile scalar.ink layers -o scalar.h)
check(compile_hoisted)
file(WRITE "${OUT}/fact.ink" "layers(x_n) = {\n    s(v)_0 = v\n    s(v)_k = s(v)_(k-1)/2 + v\n"
     "    o(a)_0 = lim s(a)\n    o(a)_k = o(a)_(k-1)/4 + 1/!(a + 2)\n    q_n = lim o(x_n)\n}\n")
set(args --compile fact.ink layers -o fact.h)
check(compile_hoisted)
set(based "    if (isnan(t1_)) return ran_ ? 0.0 : NAN;\n")
string(CONCAT walk "    double step_ = 0.0, before_ = 0.0, close_ = 0.0, closed_ = 0.0, kept_ = 0.0;\n"
       "    int    started_ = 1, stepped_ = 0, held_ = 0;\n"
       "    for (long long k_ = 1; k_ <= 100; ++k_) {\n")
holds(compile_hoisted layered.h "    double v0_[2][1] = {{arg_a}, {1.0}};\n    double v1_[2][1];\n"
      "    layered_lim0(m_, v0_, v1_);\n    double t1_ = arg_a;\n${based}${walk}"
      "        const double t_ = t1_ / 4.0 + (1.0 * v1_[0][0] + 1.0 * v1_[1][0]);\n")
holds(compile_hoisted scalar.h "    const double v0_ = scalar_lim0(m_, 0, arg_a + 1.0);\n"
      "    double t1_ = arg_a;\n${based}${walk}        const double t_ = t1_ / 4.0 + v0_;\n")
holds(compile_hoisted fact.h "    const double v0_ = fact_fact_(arg_a + 2.0);\n"
      "    double t1_ = fact_lim0(m_, 0, arg_a);\n${based}${walk}"
      "        const double t_ = t1_ / 4.0 + 1.0 / v0_;\n")

# So is the status of a limit a guard in the terms reads, asked with its truth.
file(WRITE "${OUT}/asked.ink" "layers(x_n) = {\n    s(v)_0 = v\n    s(v)_k = s(v)_(k-1)/2 + v\n"
     "    g(a) | lim s(a + 1) > 0 = 1\n    g(a) = 2\n"
     "    o(a)_0 = a\n    o(a)_k = o(a)_(k-1)/4 + g(a)\n    q_n = lim o(x_n)\n}\n")
set(args --compile asked.ink layers -o asked.h)
check(compile_hoisted)
holds(compile_hoisted asked.h "    const double v0_ = asked_lim0(m_, 0, arg_a + 1.0);\n"
      "    const double v1_ = (isnan(v0_) ? NAN : v0_ > 0.0 ? 1.0 : 0.0);\n"
      "    const double v2_ = isnan(v1_) && !(asked_lim0(m_, 1, arg_a + 1.0) != 0.0);\n"
      "    double t1_ = arg_a;\n${based}${walk}"
      "        const double t_ = t1_ / 4.0 + (v2_ != 0.0 ? NAN : v1_ == 1.0 ? 1.0 : 2.0);\n")

# A global's temporaries, read first in a limit's terms, are numbered as
# the update's, not as that limit's: two such are two names.
file(WRITE "${OUT}/twice.ink" "a = 1/2\nM = [a, 1; 1, a]\nP = (M + M)*(M + M)\nQ = (M - 2)*(M - 2)\ns(x)_0 = x\ns(x)_k = s(x)_(k-1)/2 + P[1,1]*x\nr(x)_0 = x\nr(x)_k = r(x)_(k-1)/2 + Q[1,1]*x\ny_n = lim s(n) + lim r(n)\n")
set(args --compile twice.ink -o twice.h)
check(compile_limit_globals)
holds(compile_limit_globals twice.h "    const double t0_ = m_->a + m_->a;\n"
      "    const double t2_ = m_->a - 2.0;\n")

# Reductions over an index (DESIGN.md), unrolled as a sum is, a later term
# kept where it ranks strictly higher, NaN where a term is in a header that
# writes NaN, and refused where the interpreter refuses, past max_terms or
# under grad.
set(args --compile ${CMAKE_CURRENT_LIST_DIR}/compile/reduce.ink pool -o pool.h)
check(compile_reduce)
holds(compile_reduce pool.h
      "m_->x[0][0][0] < m_->x[0][1][0] ? m_->x[0][1][0] : m_->x[0][0][0])"
      "m_->x[0][1][0] < m_->x[0][0][0] ? 2.0 : 1.0)"
      "m_->x[0][2][0] < m_->x[0][3][0] ? m_->x[0][3][0] : m_->x[0][2][0])")
file(WRITE "${OUT}/aware.ink" "r(v) | v > 0 = v\naware(x_n[j<=2]) = {\n"
     "    q_n = max_(k=1)^2 x_n[k]\n    z_n = r(x_n[1])\n}\n")
set(args --compile aware.ink aware -o aware.h)
check(compile_reduce)
holds(compile_reduce aware.h "(isnan(m_->x[0][0][0]) || isnan(m_->x[0][1][0]) ? NAN : "
      "m_->x[0][0][0] < m_->x[0][1][0] ? m_->x[0][1][0] : m_->x[0][0][0])")
file(WRITE "${OUT}/reduced.ink" "e_n = max_(k=3)^1 k*x_n\n"
     "g_n = grad_(w = x_n) max_(k=1)^3 (k*w - k^2)\nh_n = argmin_(k=0)^1000 k*x_n\n"
     "a_n = grad_(w = x_n) argmax_(k=1)^3 (k*w - k^2)\n")
set(args --compile reduced.ink)
string(CONCAT stdout "cannot compile a: a derivative of an argmax, for now\n"
       "cannot compile e: the maximum has no terms: k runs from 3 to 1\n"
       "cannot compile g: a derivative of a maximum, for now\n"
       "cannot compile h: the argmin has more than 1000 terms\n")
set(exit 1)
check(compile_reduce)

# Optional inputs (DESIGN.md): the step takes a flag after its optional
# input, and the struct keeps the flags beside its window.
set(args --compile ${CMAKE_CURRENT_LIST_DIR}/compile/optional.ink track -o track.h)
check(compile_optional)
holds(compile_optional track.h
      "TRACK_API_ void track_step(track* m_, double z_in_, int z_present_in_) {\n"
      " *     track_step(&m, z, z_present);  once for each index, the first 0\n"
      "    int z_present[4];\n" "    m_->z[0] = z_present_in_ ? z_in_ : NAN;\n")
strict(compile_optional track.h)

# An inner instance's optional input (DESIGN.md): compile/inner.ink's headers,
# the file compiled whole, a lagged sibling refused, and the reports of
# fwd, sib, dp, ow, gp, hs, gb, lt and lo; test/CMakeLists.txt checks its robot.
set(story "${CMAKE_CURRENT_LIST_DIR}/compile/inner.ink")
set(args --compile ${story} fw -o fw.h)
check(compile_inner)
holds(compile_inner fw.h
      "FW_API_ void fw_step(fw* m_, double z_in_, int z_present_in_, double w_in_) {\n"
      "        int z_present[4];\n")
strict(compile_inner fw.h)
set(args --compile ${story} loop -o loop.h)
check(compile_inner_loop)
holds(compile_inner_loop loop.h "LOOP_API_ void loop_step(loop* m_) {\n"
      "        int z_present[1];\n")
strict(compile_inner_loop loop.h)
set(args --compile ${story} alone -o alone.h)
check(compile_inner_alone)
holds(compile_inner_alone alone.h "    m_->f.e.z_present[0] = m_->f.z_present[0];\n")
strict(compile_inner_alone alone.h)
set(args --compile ${story})
set(stdout "inner.ink compiles\n")
check(compile_inner_file)
file(WRITE "${OUT}/lag.ink" "held(z_n?) = {\n    x_0 = 0\n    x_n | z_n >= 0 = z_n\n"
     "    x_n = x_(n-1)\n}\nsrc() = {\n    z_n | mod(n, 3) == 0 = n\n}\n"
     "lag() = {\n    s = src()\n    h = held(z_n = s.z_(n-1))\n}\n")
set(args --compile lag.ink lag -o lag.h)
set(stderr "inkamath: cannot compile h.x: h.z is given s.z_(n-1), whose chain may give none, read back in time, for now\n")
set(exit 1)
check(compile_inner_lag)
# The file's instance of it, both names said from the file.
file(APPEND "${OUT}/lag.ink" "lg = lag()\n")
set(args --compile lag.ink)
set(stdout "cannot compile lg.h.x: lg.h.z is given lg.s.z_(n-1), whose chain may give none, read back in time, for now\n")
set(exit 1)
check(compile_inner_lag_file)
# A slow chain held, whose status at the term held the step does not keep.
file(WRITE "${OUT}/hold.ink" "held(z_n?) = {\n    x_0 = 0\n    x_n | z_n >= 0 = z_n\n"
     "    x_n = x_(n-1)\n}\nslow(x_n) = {\n    y_m | mod(m, 3) == 0 = x_(2*m)\n}\n"
     "hold() = {\n    s = slow(x_n = n)\n    h = held(z_n = s.y_(floor(n/2)))\n}\n")
set(args --compile hold.ink hold -o hold.h)
set(stderr "inkamath: cannot compile h.x: h.z is given s.y_(...), whose chain may give none, held, for now\n")
set(exit 1)
check(compile_inner_hold)
if(CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
    # A report's first line, then a line for each pattern in any order, then
    # the guards' lines as given; ';' read as ',', which a list splits at.
    function(reported instance steps status first guards)
        set(args --check ${story} ${instance} --steps ${steps} -o ${instance}.c)
        check(check_inner_${instance})
        # Uncontracted, as test/CMakeLists.txt builds a check (C97).
        execute_process(COMMAND "${CC}" -std=c11 -O2 -ffp-contract=off ${instance}.c
                                -o ${instance} -lm
                        WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE failed
                        ERROR_VARIABLE errors OUTPUT_VARIABLE errors)
        if(failed)
            message(SEND_ERROR "check_inner_${instance}: ${instance}.c does not build\n${errors}")
            return()
        endif()
        execute_process(COMMAND "${OUT}/${instance}" WORKING_DIRECTORY "${OUT}"
                        OUTPUT_VARIABLE report RESULT_VARIABLE code TIMEOUT ${CASE_TIMEOUT})
        string(REPLACE ";" "," text "${report}")
        string(REPLACE ";" "," guards "${guards}")
        string(LENGTH "${text}" all)
        string(LENGTH "${guards}" last)
        set(bad "")
        set(head "")
        if(all LESS last)
            set(bad 1)
        else()
            math(EXPR from "${all} - ${last}")
            string(SUBSTRING "${text}" ${from} ${last} tail)
            string(SUBSTRING "${text}" 0 ${from} head)
            if(NOT "${tail}" STREQUAL "${guards}")
                set(bad 1)
            endif()
        endif()
        string(REPLACE "\n" ";" lines "${head}")
        list(REMOVE_ITEM lines "")
        list(POP_FRONT lines top)
        list(LENGTH lines got)
        list(LENGTH ARGN want)
        if(NOT code EQUAL status OR NOT got EQUAL want OR
           NOT "${top}" STREQUAL "${instance}: ${steps} steps from 0, ${first}")
            set(bad 1)
        endif()
        foreach(pattern IN LISTS ARGN)
            set(found "")
            foreach(line IN LISTS lines)
                if(line MATCHES "^${instance}\\.${pattern}$")
                    set(found 1)
                endif()
            endforeach()
            if(NOT found)
                set(bad 1)
            endif()
        endforeach()
        if(bad)
            string(REPLACE ";" "\n" each "${ARGN}")
            message(SEND_ERROR "check_inner_${instance}: exit ${code}\n${report}--- where it is to exit ${status}, say '${first}', then a line for each of\n${each}\n--- in any order, then\n${guards}")
        endif()
    endfunction()

    set(last "")
    foreach(i e k)
        string(APPEND last
               "@.${i}.d: where z is absent, 'd_n | z_n - z_(n-3) > 0 = z_n - z_(n-3)' falls to 'd_n = 0'\n"
               "@.${i}.x: where z is absent, 'x_n | z_n >= 0 = z_n' falls to 'x_n = x_(n-1)'\n")
        if(i STREQUAL "e")
            set(one "${last}")
        endif()
    endforeach()
    string(REPLACE "@" "fwd" guards "${last}")
    reported(fwd 100 0 "against exact values" "${guards}" "e\\.z: within 0" "e\\.x: within 0"
             "e\\.d: within 0" "k\\.z: within 0" "k\\.x: within 0" "k\\.d: within 0" "y: within 0")
    string(REPLACE "@" "sib" guards "${last}")
    set(nan "-?nan at 20, where the interpreter's term is not a finite number")
    reported(sib 100 1 "against exact values until 20 and inexact ones from there" "${guards}"
             "s\\.z: ${nan}" "e\\.z: ${nan}" "k\\.z: ${nan}" "e\\.x: within 0" "e\\.d: within 0"
             "k\\.x: within 0" "k\\.d: within 0")
    string(REPLACE "@" "dp.a" guards "${last}")
    set(terms "")
    foreach(s z w e.z e.x e.d k.z k.x k.d y)
        string(REPLACE "." "\\." s "a.${s}")
        list(APPEND terms "${s}: within 0")
    endforeach()
    reported(dp 100 0 "against exact values" "${guards}" ${terms})
    string(REPLACE "@" "ow" guards "${one}")
    reported(ow 100 0 "against exact values" "${guards}" "q: within 0" "e\\.z: within 0"
             "e\\.x: within 0" "e\\.d: within 0")
    string(REPLACE "@" "gp" guards "${one}")
    reported(gp 100 0 "against exact values" "${guards}" "s\\.z: within 0" "e\\.z: within 0"
             "e\\.x: within 0" "e\\.d: within 0")
    reported(hs 100 0 "against exact values"
             "hs.e.d: where z is absent, 'd_n | z_n - z_(n-3) > 0 = z_n - z_(n-3)' falls to 'd_n = -1'\n"
             "q: within 0" "e\\.z: within 0" "e\\.d: within 0")
    reported(gb 100 0 "against exact values"
             "gb.e.x: where z is absent, 'x_n | z_n >= 0 = z_n' falls to 'x_n = x_(n-1)'\n"
             "s\\.y: within 0" "s\\.z: within 0" "e\\.z: within 0" "e\\.x: within 0")
    set(guards "")
    foreach(i b e)
        string(APPEND guards
               "lt.${i}.d: where z is absent, 'd_n | z_(n-3) >= 0 and z_n > z_(n-3) = z_n - z_(n-3)' falls to 'd_n = 0'\n"
               "lt.${i}.x: where z is absent, 'x_n | z_n >= 0 = z_n' falls to 'x_n = -1'\n")
    endforeach()
    reported(lt 100 0 "against exact values" "${guards}" "s\\.y: within 0"
             "s\\.z: within 0, from 1" "e\\.z: within 0" "e\\.x: within 0" "e\\.d: within 0"
             "b\\.z: within 0" "b\\.x: within 0" "b\\.d: within 0")
    string(REPLACE "@" "lo.f" guards "${last}")
    string(APPEND guards "lo.p.c: where z is absent, 'c_n | [1 0]*z_(n-3) < 0 = [1 0]*z_(n-3)' falls to 'c_n = 1'\n")
    set(terms "")
    foreach(s f.w f.e.x f.e.d f.k.z f.k.x f.k.d f.y p.c y)
        string(REPLACE "." "\\." s "${s}")
        list(APPEND terms "${s}: within 0")
    endforeach()
    foreach(s f.z f.e.z p.z)
        string(REPLACE "." "\\." s "${s}")
        list(APPEND terms "${s}: no value compared, the step NaN where the interpreter gives none, as at 0: lo\\.${s}_0 is absent")
    endforeach()
    reported(lo 100 0 "against exact values" "${guards}" ${terms})
endif()

# grad's order compiled (DESIGN.md): compile/hessian.ink's instances each
# within 0 of the interpreter, and the nests still refused.
set(story "${CMAKE_CURRENT_LIST_DIR}/compile/hessian.ink")
set(args --compile ${story} lag -o lag.h)
check(compile_hessian)
strict(compile_hessian lag.h)

# A guard reading a matrix limit's status, built as C++ too (C393).
foreach(flags "robot.h" "float/robot.h;--float")
    list(POP_FRONT flags header)
    set(args --compile ${CMAKE_CURRENT_LIST_DIR}/compile/inner.ink loop ${flags} -o ${header})
    check(compile_status_strict)
    strict(compile_status_strict ${header})
endforeach()

set(inexact "against exact values until 1 and inexact ones from there")
foreach(checked IN ITEMS "p:${inexact}:M C G w q" "h:${inexact}:k s M C G w q"
                         "b:against exact values:d q")
    string(REPLACE ":" ";" checked "${checked}")
    list(POP_FRONT checked instance against terms)
    set(args --check ${story} ${instance} -o ${instance}.c)
    check(check_hessian_${instance})
    if(NOT CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
        continue()
    endif()
    # Uncontracted, as test/CMakeLists.txt builds a check (C97).
    execute_process(COMMAND "${CC}" -std=c11 -O2 -ffp-contract=off ${instance}.c
                            -o ${instance} -lm
                    WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE failed
                    ERROR_VARIABLE errors OUTPUT_VARIABLE errors)
    if(failed)
        message(SEND_ERROR "check_hessian_${instance}: ${instance}.c does not build\n${errors}")
        continue()
    endif()
    execute_process(COMMAND "${OUT}/${instance}" WORKING_DIRECTORY "${OUT}"
                    OUTPUT_VARIABLE report RESULT_VARIABLE code TIMEOUT ${CASE_TIMEOUT})
    # A list splits at ';', which a line's estimate holds.
    string(REPLACE ";" "," lines "${report}")
    string(REPLACE "\n" ";" lines "${lines}")
    list(REMOVE_ITEM lines "")
    list(POP_FRONT lines first)
    separate_arguments(terms)
    list(LENGTH terms want)
    list(LENGTH lines got)
    set(bad "")
    if(NOT code EQUAL 0 OR NOT "${first}" STREQUAL "${instance}: 100 steps from 0, ${against}"
       OR NOT got EQUAL want)
        set(bad 1)
    endif()
    foreach(s IN LISTS terms)
        set(found "")
        foreach(line IN LISTS lines)
            if(line MATCHES "^${instance}\\.${s}: within 0(, from 1)?(,.*)?$")
                set(found 1)
            endif()
        endforeach()
        if(NOT found)
            set(bad 1)
        endif()
    endforeach()
    if(bad)
        message(SEND_ERROR "check_hessian_${instance}: exit ${code}\n${report}--- where each of ${terms} is to be within 0")
    endif()
endforeach()

file(WRITE "${OUT}/hessian_refused.ink"
     "a_n = grad_(t = x_n)^3 t^5\nb_n = grad_(s = x_n, r = 2) grad_(t = s) t^2*r\n"
     "c_n = grad_(t = x_n)^2 lim p(t)\nd_n = grad_(v = [x_n 1; 2 3])^2 v[1,1]*v[2,2]\n"
     "e_n = grad_(v = [x_n; 1])^3 v[1]^4\nf_n = grad_(t = x_n)^2 grad_(s = 2) t^2*s^2\n"
     "g_n = grad_(v = [x_n; 1])^2 3*x_n\n"
     "p(r)_0 = 1\np(r)_k = r*p(r)_(k-1)/4 + 1\n")
set(args --compile hessian_refused.ink)
set(stdout "cannot compile a: a derivative of a derivative of a derivative, for now
cannot compile b: a derivative of a derivative by several names, for now
cannot compile c: a second derivative through a limit, for now
cannot compile d: a derivative of order 2 with respect to a matrix, for now
cannot compile e: a derivative of order 3 with respect to a vector, for now
cannot compile f: a derivative of a derivative of a derivative, for now
cannot compile g: grad's expression does not read v
")
set(exit 1)
check(compile_hessian_refused)

# The forward form (DESIGN.md): each model of compile/forward.ink compiled to
# its shift's header but for the name and where its comments wrap, an
# instance of each checked to its shift's report, the delay refused as its
# shift is, and a cell of every term named as written.
set(story "${CMAKE_CURRENT_LIST_DIR}/compile/forward.ink")
foreach(model IN ITEMS plant nesterov second acc sampled held tracker)
    foreach(written IN ITEMS ${model} shifted${model})
        set(args --compile ${story} ${written} -o ${written}.h)
        check(compile_forward_${written})
    endforeach()
    strict(compile_forward_${model} ${model}.h)
    file(READ "${OUT}/${model}.h" forward)
    file(READ "${OUT}/shifted${model}.h" shifted)
    string(TOUPPER "${model}" upper)
    string(REPLACE "shifted${model}" "${model}" shifted "${shifted}")
    string(REPLACE "SHIFTED${upper}" "${upper}" shifted "${shifted}")
    foreach(header forward shifted)
        string(REGEX REPLACE "\n \\*" "\n" ${header} "${${header}}")
        string(REGEX REPLACE "[ \n]+" " " ${header} "${${header}}")
    endforeach()
    if(NOT forward STREQUAL shifted)
        message(SEND_ERROR "compile_forward_${model}: ${model}.h is not shifted${model}.h but for the name and its spacing")
    endif()
endforeach()

set(args --compile ${story} delay)
set(stdout "cannot compile c: c_0 reads x_-1, before the stream, where x has no history\n")
set(exit 1)
check(compile_forward_delay)

set(args --compile ${story} tapped)
set(stdout "cannot compile v: v_0 and v_(n+1)[2,1] both give a cell of v_0; write v_0[2,1] to say which\n")
set(exit 1)
check(compile_forward_tapped)
set(args --compile ${story} ranked)
set(stdout "cannot compile v: a clause for one cell of v_(n+1), a 2x2x2 tensor, names its slice, row and column\n")
set(exit 1)
check(compile_forward_ranked)

if(CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
    foreach(instance IN ITEMS plt ball pair total tally hold track)
        foreach(written IN ITEMS ${instance} shifted${instance})
            set(args --check ${story} ${written} -o ${written}.c)
            check(check_forward_${written})
            execute_process(COMMAND "${CC}" -std=c11 -O2 ${written}.c -o ${written} -lm
                            WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE failed
                            ERROR_VARIABLE errors OUTPUT_VARIABLE errors)
            if(failed)
                message(SEND_ERROR "check_forward_${written}: ${written}.c does not build\n${errors}")
            endif()
            execute_process(COMMAND "${OUT}/${written}" WORKING_DIRECTORY "${OUT}"
                            OUTPUT_VARIABLE report_${written} RESULT_VARIABLE code_${written}
                            TIMEOUT ${CASE_TIMEOUT})
        endforeach()
        string(REPLACE "shifted${instance}" "${instance}" shifted "${report_shifted${instance}}")
        if(NOT code_${instance} EQUAL 0 OR NOT code_shifted${instance} EQUAL 0 OR
           NOT "${report_${instance}}" STREQUAL "${shifted}" OR
           NOT "${shifted}" MATCHES "^${instance}: 100 steps from 0, against exact values\n")
            message(SEND_ERROR "check_forward_${instance}: exit ${code_${instance}}\n${report_${instance}}--- where shifted${instance}'s, exit ${code_shifted${instance}}, is\n${report_shifted${instance}}")
        endif()
    endforeach()

    # Where an optional input is absent, the clauses quoted as written.
    set(args --check ${story} seen -o seen.c)
    check(check_forward_seen)
    execute_process(COMMAND "${CC}" -std=c11 -O2 seen.c -o seen -lm WORKING_DIRECTORY "${OUT}"
                    RESULT_VARIABLE failed ERROR_VARIABLE errors OUTPUT_VARIABLE errors)
    execute_process(COMMAND "${OUT}/seen" WORKING_DIRECTORY "${OUT}" OUTPUT_VARIABLE report
                    RESULT_VARIABLE code TIMEOUT ${CASE_TIMEOUT})
    set(expected "seen: 100 steps from 0, against exact values
seen.x: within 0
seen.x: where z is absent, 'x_(n+1) | z_(n+1) >= 0 = z_(n+1)' falls to 'x_(n+1) = x_n'
")
    if(failed OR NOT code EQUAL 0 OR NOT "${report}" STREQUAL "${expected}")
        message(SEND_ERROR "check_forward_seen: exit ${code}\n${errors}${report}--- expected\n${expected}")
    endif()
endif()

endif()

# A sequence with parameters walked (DESIGN.md, done after the phases). Each
# model of compile/seqcells.ink compiled, strict, stencil in float too; the
# walk named once and called once for one term; rho's bisection walked, by
# its headers' sizes; an instance of each checked to the lines its comment
# gives; and what stays refused in its words.
if(NOT PART OR PART GREATER 4)
    file(MAKE_DIRECTORY "${OUT}")
    set(story "${CMAKE_CURRENT_LIST_DIR}/compile/seqcells.ink")
    if(NOT PART OR PART EQUAL 5)
        foreach(model IN ITEMS stencil:heat ends:ends parts:parts walked:walked stab:stab
                               stab4:stab4 fourfold:fourfold halving:halving shapes:shapes
                               counted:counted folded:folded)
            string(REPLACE ":" ";" model "${model}")
            list(GET model 1 header)
            list(GET model 0 model)
            set(args --compile ${story} ${model} -o ${header}.h)
            check(compile_seqcells_${model})
            strict(compile_seqcells_${model} ${header}.h)
        endforeach()
        set(args --compile ${story} stencil --float -o heatf.h)
        check(compile_seqcells_float)
        strict(compile_seqcells_float heatf.h -Wdouble-promotion)

        # One walk for a and b, the index its argument; one call for e's two cells.
        holds(compile_seqcells_stencil heat.h "static inline double heat_walk0("
              "heat_walk0(m_, 3.0, " "heat_walk0(m_, (double)m_->index_, ")
        file(READ "${OUT}/heat.h" text)
        string(FIND "${text}" "heat_walk1" at)
        if(NOT at EQUAL -1)
            message(SEND_ERROR "compile_seqcells_stencil: heat.h defines a second walk")
        endif()
        file(READ "${OUT}/ends.h" text)
        string(REGEX MATCHALL "ends_walk0\\(m_" calls "${text}")
        list(LENGTH calls count)
        if(NOT count EQUAL 1)
            message(SEND_ERROR "compile_seqcells_ends: ends.h calls its walk ${count} times, not once")
        endif()
        # rhou's 256 terms walked, as they reach hurwitzr by cells: written out,
        # some 2.2 and 4.7 MB.
        foreach(bound IN ITEMS stab:200000 stab4:400000)
            string(REPLACE ":" ";" bound "${bound}")
            list(GET bound 0 header)
            list(GET bound 1 bytes)
            holds(compile_seqcells_${header} ${header}.h "static inline double ${header}_walk")
            file(SIZE "${OUT}/${header}.h" size)
            if(NOT size LESS bytes)
                message(SEND_ERROR "compile_seqcells_${header}: ${header}.h is ${size} bytes, not under ${bytes}")
            endif()
        endforeach()
        # A clause behind a guard that folds false reaches nothing: ff_3 written
        # out as today, and no walk.
        holds(compile_seqcells_folded folded.h [[    const double t0_ = m_->x[0] / 2.0;
    const double t1_ = t0_ / 2.0;
    m_->y[0] = t1_ / 2.0;
]])
        file(READ "${OUT}/folded.h" text)
        string(FIND "${text}" "_walk" at)
        if(NOT at EQUAL -1)
            message(SEND_ERROR "compile_seqcells_folded: folded.h walks")
        endif()
        # A walked read at a parameter K reads it from the struct, which the host
        # may change, where a read written out compiles it in.
        holds(compile_seqcells_counted counted.h "counted_walk0(m_, m_->K, ")
        file(READ "${OUT}/counted.h" text)
        string(FIND "${text}" "cannot change" at)
        if(NOT at EQUAL -1)
            message(SEND_ERROR "compile_seqcells_counted: counted.h compiles K in")
        endif()
        # An index of parameters alone, through a function or a value of them,
        # is compiled in where the read is written out, as before; one reading
        # n too is walked, N read from the struct.
        file(WRITE "${OUT}/keyed.ink" "q(x)_0 = x\nq(x)_k = q(x)_(k-1)/2 + 1\n"
             "fixedk(N = 4, x_n) = {\n    M = N + 1\n    a_n = q(x_n)_(floor(N/2))\n"
             "    b_n = q(x_n)_M\n}\nmovedk(N = 4, x_n) = {\n    c_n = q(x_n)_(N + n)\n}\n")
        foreach(model IN ITEMS fixedk movedk)
            set(args --compile keyed.ink ${model} -o ${model}.h)
            check(compile_seqcells_${model})
        endforeach()
        holds(compile_seqcells_fixedk fixedk.h "cannot change: N.")
        holds(compile_seqcells_movedk movedk.h "movedk_walk0(m_, m_->N + (double)m_->index_, ")
        file(READ "${OUT}/fixedk.h" text)
        string(FIND "${text}" "_walk" at)
        if(NOT at EQUAL -1)
            message(SEND_ERROR "compile_seqcells_fixedk: fixedk.h walks")
        endif()

        # a's terms change size; b reads its own term at floor(k/2), c with other
        # arguments; d is a second derivative and e's index moves with t; f has
        # no base clause; g is past the interpreter's fill and k below y's base,
        # both constants; m is a tensor.
        file(WRITE "${OUT}/seqcells_refused.ink"
             "a_n = w([x_n; 1; 2])_n\nb_n = h(x_n)_n\nc_n = r(x_n)_n\n"
             "d_n = grad_(t = x_n)^2 q(t)_n\ne_n = grad_(t = x_n) q(t)_(floor(t))\nf_n = p(x_n)_n\n"
             "g_n = q(x_n)_20000000\nk_n = y(x_n)_(-1)\nm_n = tq(x_n)_n\n"
             "w(a)_0 = a\nw(a[j<=m])_t[r<=m-t] = w(a)_(t-1)[r]\n"
             "h(s)_0 = 0\nh(s)_k = 2*h(s)_(floor(k/2)) + s\nr(x)_0 = x\nr(x)_k = r(x/2)_(k-1) + 1\n"
             "q(x)_0 = x\nq(x)_k = q(x)_(k-1)/2 + 1\np(x)_k = p(x)_(k-1)/2\n"
             "y(x)_0 = [x; 1]\ny(x)_k[j<=2] = y(x)_(k-1)[j]*x\n"
             "tq(x)_0 = [1;; 0]\ntq(x)_k = tq(x)_(k-1)*x/2 + [1;; x]\n")
        set(args --compile seqcells_refused.ink)
        string(CONCAT stdout "cannot compile a: a walk of w, whose terms change size\n"
                      "cannot compile b: h_(...): an index other than a whole multiple of k plus a constant\n"
                      "cannot compile c: r's term read with other arguments than its own\n"
                      "cannot compile d: a second derivative through a walk of q, for now\n"
                      "cannot compile e: an index that moves with t, for now\n"
                      "cannot compile f: p has no base clause to walk from\n"
                      "cannot compile g: q_20000000 is 20000000 terms from its base, and a fill stops at 10000000\n"
                      "cannot compile k: y has no clause for index -1\n"
                      "cannot compile m: a tensor in a walk of tq, for now\n")
        set(exit 1)
        check(compile_seqcells_refused)
        # Past the fill by more than an int holds, from a base below 0.
        file(WRITE "${OUT}/seqcells_far.ink"
             "a_n = v(x_n)_2147483647\nb_n = r(x_n)_2147483647\n"
             "v(x)_(-1) = [x; 1]\nv(x)_k[j<=2] = v(x)_(k-1)[j]*x\nr(x)_(-1) = x\nr(x)_k = r(x)_(k-1)/2\n")
        set(args --compile seqcells_far.ink)
        string(CONCAT stdout
               "cannot compile a: v_2147483647 is 2147483648 terms from its base, and a fill stops at 10000000\n"
               "cannot compile b: r_2147483647 is 2147483648 terms from its base, and a fill stops at 10000000\n")
        set(exit 1)
        check(compile_seqcells_far)
        # A matrix at the index, walked or a closed form, as the interpreter says.
        file(WRITE "${OUT}/seqcells_matrix.ink" "a_n = q(x_n)_([n; 1])\nb_n = p(x_n)_([n; 1])\n"
             "q(x)_0 = x\nq(x)_k = q(x)_(k-1)/2 + 1\np(x)_k = x*k\n")
        set(args --compile seqcells_matrix.ink)
        string(CONCAT stdout "cannot compile a: a matrix is not a single value\n"
                      "cannot compile b: a matrix is not a single value\n")
        set(exit 1)
        check(compile_seqcells_matrix)

    endif()
    if(NOT CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
        return()
    endif()
    if(NOT PART OR PART EQUAL 5)
        # counted stepped by a host at x = 1: (M^3 bump(1))[4], then K set to 5.
        file(WRITE "${OUT}/counted_host.c" "#include <stdio.h>\n#include \"counted.h\"\n"
             "int main(void) {\n    counted m;\n    counted_init(&m);\n    counted_step(&m, 1.0);\n"
             "    printf(\"%.17g\\n\", m.a[0]);\n    m.K = 5.0;\n    counted_update(&m);\n"
             "    counted_step(&m, 1.0);\n    printf(\"%.17g\\n\", m.a[0]);\n    return 0;\n}\n")
        execute_process(COMMAND "${CC}" -std=c11 -O2 counted_host.c -o counted_host -lm
                        WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE failed
                        ERROR_VARIABLE errors OUTPUT_VARIABLE errors)
        if(failed)
            message(SEND_ERROR "compile_seqcells_counted: its host not built\n${errors}")
        else()
            execute_process(COMMAND "${OUT}/counted_host" WORKING_DIRECTORY "${OUT}"
                            OUTPUT_VARIABLE got RESULT_VARIABLE code TIMEOUT ${CASE_TIMEOUT})
            if(NOT code EQUAL 0 OR NOT "${got}" STREQUAL "0.65625\n0.6005859375\n")
                message(SEND_ERROR "compile_seqcells_counted: exit ${code}, the host printed\n${got}")
            endif()
        endif()
    endif()
    set(float "")
    set(exits 0)
    # Each instance checked for as many steps as cross its transitions, in
    # two parts under the sanitizers' 240 s: quad's check takes 114 s there
    # and degen's 39, stab4's compile 60, fourfold's 32 and pair's check 30.
    set(inexact "steps from 0, against exact values until 0 and inexact ones from there")
    set(about "(, the interpreter's terms about [0-9.e+-]+ from the exact ones)?")
    set(within0 ": within 0${about}")
    # Under 1e-15 and under 1e-13, as %.2g prints them.
    set(e15 ": within (0|[0-9.]+e-(1[6-9]|[2-9][0-9]|[1-9][0-9][0-9]))${about}")
    set(e13 ": within (0|[0-9.]+e-(1[4-9]|[2-9][0-9]|[1-9][0-9][0-9]))${about}")
    if(NOT PART OR PART EQUAL 5)
        set(count 40)
        seqcells(heat heat "heat: 40 ${inexact}" "heat\\.a${within0}" "heat\\.b${within0}")
        seqcells(sens sens "sens: 40 steps from 0, against exact values" "sens\\.g${e15}"
                 "sens\\.v${within0}")
        set(count 30)
        seqcells(rim rim "rim: 30 ${inexact}" "rim\\.e${within0}")
        seqcells(idx idx "idx: 30 ${inexact}" "idx\\.a${within0}" "idx\\.b${within0}"
                 "idx\\.c${within0}" "idx\\.d${within0}" "idx\\.e${e15}")
        seqcells(forms forms "forms: 30 ${inexact}" "forms\\.a${within0}" "forms\\.b${within0}"
                 "forms\\.c${within0}" "forms\\.d${within0}")
        seqcells(fixed fixed "fixed: 30 ${inexact}" "fixed\\.y${within0}")
        # In float, the stencil's floats under a hundred units of the
        # interpreter's doubles.
        set(count 40)
        set(float --float)
        set(units ": within [0-9.e+-]+, ([0-9](\\.[0-9])?|[1-9][0-9]) units of a float${about}")
        seqcells(heat floated "heat: 40 steps from 0 in float, against exact values until 0 and inexact ones from there"
                 "heat\\.a${units}" "heat\\.b${units}")
    endif()
    # These five lines and no other: guard lines the implementation must add
    # amend the pattern, with their reason. pair in part 5, quad in 6.
    set(count 10)
    set(float "")
    set(instances "")
    if(NOT PART OR PART EQUAL 5)
        list(APPEND instances pair)
    endif()
    if(NOT PART OR PART EQUAL 6)
        list(APPEND instances quad)
    endif()
    foreach(instance IN LISTS instances)
        seqcells(${instance} ${instance} "${instance}: 10 ${inexact}"
                 "${instance}\\.G${within0}" "${instance}\\.a${e13}" "${instance}\\.h${within0}"
                 "${instance}\\.r${e13}" "${instance}\\.s${within0}")
    endforeach()
    if(NOT PART OR PART EQUAL 6)
        # C275's limit in the step: rho of a fourfold 1/3 bisected in doubles,
        # 0.333386 where the interpreter's is 1/3's double, from the first step.
        set(count 100)
        set(exits 1)
        set(gives "where the interpreter gives ~0\\.33333333333333331")
        set(one "(, the interpreter's term about [0-9.e+-]+ from the exact one)?")
        seqcells(degen degen "degen: 100 ${inexact}" "degen\\.r: 0\\.33338[0-9]+ at 0, ${gives}${one}")
        set(exits 0)
    endif()
endif()

# Definitions by cells as loops (DESIGN.md, done after the phases). Each model of compile/cellloops.ink compiled, strict, the 32x32
# tank in float too; its loops, index tables and sums held to the rule's
# lines, a temporary's number written '#'; the headers' sizes; an instance
# of each checked to the lines its comment gives; and what stays refused in
# its words. Three parts under the sanitizers' 240 s: 7 the headers' lines
# and the refusals, 8 the 32x32 instances, 9 the full tank and the 32x32
# tank in float.
if(NOT PART OR PART GREATER 6)
    file(MAKE_DIRECTORY "${OUT}")
    set(story "${CMAKE_CURRENT_LIST_DIR}/compile/cellloops.ink")
    set(float "")
    set(exits 0)
    set(inexact "steps from 0, against exact values until 0 and inexact ones from there")
    set(about "(, the interpreter's terms about [0-9.e+-]+ from the exact ones)?")
    set(within0 ": within 0${about}")
    # A header's table of 'count' ints at file scope, each 'expression' at k
    # from 0, clamped to 'low' and 'high'.
    function(table out module count expression low high)
        set(cells "")
        math(EXPR last "${count} - 1")
        foreach(k RANGE ${last})
            string(REPLACE "k" "${k}" cell "${expression}")
            math(EXPR cell "${cell}")
            if(cell LESS low)
                set(cell ${low})
            elseif(cell GREATER high)
                set(cell ${high})
            endif()
            list(APPEND cells "${cell}")
        endforeach()
        list(JOIN cells ", " cells)
        set(${out} "\nstatic const int ${module}_table#_[${count}] = {${cells}};\n" PARENT_SCOPE)
    endfunction()
    # How many times a header holds a text.
    function(count out file text)
        file(READ "${OUT}/${file}" header)
        set(n 0)
        string(FIND "${header}" "${text}" at)
        while(NOT at EQUAL -1)
            math(EXPR n "${n} + 1")
            string(LENGTH "${text}" length)
            math(EXPR at "${at} + ${length}")
            string(SUBSTRING "${header}" ${at} -1 header)
            string(FIND "${header}" "${text}" at)
        endwhile()
        set(${out} ${n} PARENT_SCOPE)
    endfunction()

    if(NOT PART OR PART EQUAL 7)
        foreach(model IN ITEMS forward32 nested32 flattened32 clamped edge edgeout act guarded pull)
            set(args --compile ${story} ${model} -o ${model}.h)
            check(compile_cellloops_${model})
            strict(compile_cellloops_${model} ${model}.h)
        endforeach()
        file(MAKE_DIRECTORY "${OUT}/float")
        foreach(model IN ITEMS forward32 nested32 flattened32 edge)
            set(args --compile ${story} ${model} --float -o float/${model}.h)
            check(compile_cellloops_float)
            strict(compile_cellloops_float float/${model}.h -Wdouble-promotion)
        endforeach()

        # The step's cells, one loop nest its body once; the periodic wraps two
        # tables, each read at both places; c's default one loop in init; the
        # term copied by increment 3's loop. Written out, 250,819 bytes.
        table(lt forward32 32 "(k + 31) % 32" 0 31)
        table(rt forward32 32 "(k + 1) % 32" 0 31)
        like(compile_cellloops_forward forward32.h "${lt}" "${rt}"
             [=[    double t#_[32][32];
    for (int i1_ = 0; i1_ < 32; ++i1_)
        for (int j1_ = 0; j1_ < 32; ++j1_) t#_[i1_][j1_] = 2.0 * m_->u[0][i1_][j1_] + (0.0 - m_->p[0][i1_][j1_]) + m_->c[i1_][j1_] * m_->c[i1_][j1_] * (m_->u[0][forward32_table#_[i1_]][j1_] + m_->u[0][forward32_table#_[i1_]][j1_] + m_->u[0][i1_][forward32_table#_[j1_]] + m_->u[0][i1_][forward32_table#_[j1_]] + (0.0 - 4.0 * m_->u[0][i1_][j1_]));
]=] [=[    for (int i_ = 0; i_ < 32; ++i_)
        for (int j_ = 0; j_ < 32; ++j_) m_->c[i_][j_] = 0.25;
]=] [=[    for (int i_ = 0; i_ < 32; ++i_)
        for (int j_ = 0; j_ < 32; ++j_) m_->field[0][i_][j_] = t#_[i_][j_];
]=])
        count(tables forward32.h "static const int forward32_table")
        count(nests forward32.h "for (int i1_ = 0;")
        if(NOT tables EQUAL 2 OR NOT nests EQUAL 1)
            message(SEND_ERROR "compile_cellloops_forward: forward32.h declares ${tables} tables and ${nests} nests of cells, not 2 and 1")
        endif()
        smaller(compile_cellloops_forward forward32.h 8000)
        # misfit's two sums, one loop in the other's, each from -0.0, so that its
        # first term is the sum's exactly, as the interpreter's sum begins.
        # Written out, 520,087 bytes.
        like(compile_cellloops_nested nested32.h [=[    double t#_ = -0.0;
    for (int k1_ = 0; k1_ < 32; ++k1_) {
        double t#_ = -0.0;
        for (int k2_ = 0; k2_ < 32; ++k2_) {
            const double t#_ = t#_[k1_][k2_] + (0.0 - m_->y[k1_][k2_]);
            t#_ = t#_ + t#_ * t#_ / 2.0;
        }
        t#_ = t#_ + t#_;
    }
    m_->objective[0] = t#_;
]=])
        smaller(compile_cellloops_nested nested32.h 10000)
        # flatmisfit's sum, its row and column two tables of 1024, each read for
        # v and for y; refused today.
        table(rows flattened32 1024 "k / 32" 0 31)
        table(cols flattened32 1024 "k % 32" 0 31)
        like(compile_cellloops_flattened flattened32.h "${rows}" "${cols}"
             [=[    double t#_ = -0.0;
    for (int k1_ = 0; k1_ < 1024; ++k1_) {
        const double t#_ = t#_[flattened32_table#_[k1_]][flattened32_table#_[k1_]] + (0.0 - m_->y[flattened32_table#_[k1_]][flattened32_table#_[k1_]]);
        t#_ = t#_ + t#_ * t#_ / 2.0;
    }
    m_->objective[0] = t#_;
]=])
        count(tables flattened32.h "static const int flattened32_table")
        if(NOT tables EQUAL 4)
            message(SEND_ERROR "compile_cellloops_flattened: flattened32.h declares ${tables} tables, not 4")
        endif()
        smaller(compile_cellloops_flattened flattened32.h 18000)
        like(compile_cellloops_float float/forward32.h "    float t#_[32][32];\n")
        like(compile_cellloops_float float/nested32.h "    float t#_ = -0.0f;\n")
        # A placed operand in float, cast, which -Wdouble-promotion holds clean.
        like(compile_cellloops_float float/edge.h
             "for (int k2_ = 0; k2_ < 143; ++k2_) t#_ = t#_ + (float)(k1_ + 1) * m_->y[0][k2_][0];\n")

        # A model's own sequence by cells, the clamps two tables, and a sum over
        # its looped term.
        table(below clamped 1024 "k - 1" 0 1023)
        table(above clamped 1024 "k + 1" 0 1023)
        like(compile_cellloops_clamped clamped.h "${below}" "${above}"
             [=[    double t#_[1024][1];
    for (int i1_ = 0; i1_ < 1024; ++i1_) t#_[i1_][0] = m_->y[0][i1_][0] + (m_->y[0][clamped_table#_[i1_]][0] + (0.0 - 2.0 * m_->y[0][i1_][0]) + m_->y[0][clamped_table#_[i1_]][0]) / 4.0;
]=] [=[    double t#_ = -0.0;
    for (int k1_ = 0; k1_ < 1024; ++k1_) t#_ = t#_ + m_->x[0][k1_][0];
]=])
        # The threshold: edge's t and w, of 1001, loops; edgeout's x and s, of
        # 1000, written out as today, its v and u loops of two indices, their
        # places affine in C, and its z written out, its place not affine.
        table(wrap edge 1001 "k % 1000" 0 999)
        like(compile_cellloops_edge edge.h "${wrap}"
             [=[    for (int k1_ = 0; k1_ < 1001; ++k1_) t#_ = t#_ + m_->y[0][edge_table#_[k1_]][0];
]=] [=[    double t#_ = -0.0;
    for (int k1_ = 0; k1_ < 7; ++k1_) {
        double t#_ = -0.0;
        for (int k2_ = 0; k2_ < 143; ++k2_) t#_ = t#_ + (double)(k1_ + 1) * m_->y[0][k2_][0];
        t#_ = t#_ + t#_;
    }
]=])
        count(nests edge.h "for (int k1_ = 0;")
        if(NOT nests EQUAL 2)
            message(SEND_ERROR "compile_cellloops_edge: edge.h holds ${nests} sums, not 2")
        endif()
        holds(compile_cellloops_edgeout edgeout.h
              "    m_->x[0][999][0] = m_->y[0][998][0] + m_->y[0][999][0];\n"
              "    m_->s[0] = m_->x[0][0][0] + m_->x[0][1][0] + m_->x[0][2][0] + "
              "    m_->z[0] = m_->y[0][1][0] + m_->y[0][2][0] + m_->y[0][3][0] + "
              " + m_->y[0][500][0] + m_->y[0][501][0] + (m_->y[0][2][0] + m_->y[0][4][0] + ")
        like(compile_cellloops_edgeout edgeout.h [=[    double t#_ = -0.0;
    for (int k1_ = 0; k1_ < 2; ++k1_) {
        double t#_ = -0.0;
        for (int k2_ = 0; k2_ < 501; ++k2_) t#_ = t#_ + m_->y[0][k1_ + k2_][0];
        t#_ = t#_ + t#_;
    }
]=] [=[        for (int k2_ = 0; k2_ < 91; ++k2_) t#_ = t#_ + m_->y[0][90 * k1_ + k2_][0];
]=])
        count(cells edgeout.h "i1_")
        count(sums edgeout.h "for (int k1_ = 0;")
        if(NOT cells EQUAL 0 OR NOT sums EQUAL 2)
            message(SEND_ERROR "compile_cellloops_edgeout: edgeout.h holds ${cells} reads of a cell's loop and ${sums} sums, not 0 and 2")
        endif()
        # A guard reading what moves, once in the body; a placed subtrahend
        # added negated, as written out.
        like(compile_cellloops_act act.h [=[    double t#_[1024][1];
    for (int i1_ = 0; i1_ < 1024; ++i1_) t#_[i1_][0] = m_->y[0][i1_][0] > 0.5 ? m_->y[0][i1_][0] : 0.0;
]=] "for (int i1_ = 0; i1_ < 1024; ++i1_) t#_[i1_][0] = m_->y[0][i1_][0] + (double)(-i1_);\n")
        # A guard reading its place, and a definition under grad, written out.
        holds(compile_cellloops_guarded guarded.h "    m_->g[0][1023][0] = m_->y[0][1022][0];\n")
        foreach(header IN ITEMS guarded pull)
            count(cells ${header}.h "i1_")
            count(sums ${header}.h "k1_")
            if(NOT cells EQUAL 0 OR NOT sums EQUAL 0)
                message(SEND_ERROR "compile_cellloops_${header}: ${header}.h holds a loop's index")
            endif()
        endforeach()

        # b's place reads both of its loops, not affinely; c reads k in an
        # exponent and d in a call of what moves, each compiling written out at
        # a few terms, so that its words are the loop's alone; e reads past y;
        # f's inner bound reads i; g is a reduction, as today; h reads a limit;
        # m's terms are matrices, which no accumulator holds yet.
        file(WRITE "${OUT}/cellloops_refused.ink"
             "refused(x_n, y_n[i<=1600]) = {\n"
             "    a_n = grad_(t = x_n) sum_(k=1)^1001 t/k\n"
             "    b_n = sum_(i=1)^1001 sum_(j=1)^2 y_n[mod(i*j, 1600) + 1]\n"
             "    c_n = sum_(k=1)^1001 x_n^k\n    d_n = sum_(k=1)^1001 max(x_n, k)\n"
             "    e_n = sum_(k=1)^1001 y_n[k + 600]\n    f_n = sum_(i=1)^1001 sum_(j=1)^i x_n/j\n"
             "    g_n = max_(k=1)^1001 k*x_n\n    h_n = sum_(k=1)^1001 lim r(x_n/k)\n"
             "    m_n = sum_(k=1)^1001 x_n*[1; 2]/k\n"
             "    r(x)_0 = x\n    r(x)_k = r(x)_(k-1)/2 + 1\n}\n")
        set(args --compile cellloops_refused.ink refused)
        string(CONCAT stdout "cannot compile a: a sum or a product of more than 1000 terms under grad, for now\n"
                      "cannot compile b: a sum or a product of more than 1000 terms whose term reads y_n at a place that is no sum of whole multiples of i and j and a constant, for now\n"
                      "cannot compile c: a sum or a product of more than 1000 terms whose term reads k in an exponent, for now\n"
                      "cannot compile d: a sum or a product of more than 1000 terms whose term reads k in a call of max, for now\n"
                      "cannot compile e: row 1601 is outside a 1600x1 matrix\n"
                      "cannot compile f: a sum or a product of more than 1000 terms whose inner sum's bounds read i, for now\n"
                      "cannot compile g: the maximum has more than 1000 terms\n"
                      "cannot compile h: a sum or a product of more than 1000 terms whose term reads a limit, for now\n"
                      "cannot compile m: a sum or a product of more than 1000 terms whose terms are 2x1 matrices, for now\n")
        set(exit 1)
        check(compile_cellloops_refused)
        # A loop's own reason, not one met compiling a value again at a place.
        file(WRITE "${OUT}/cellloops_words.ink"
             "words(x_n[i<=1100]) = {\n"
             "    p_n[i<=1100] = sum_(k=1)^1001 x_n[k]^(mod(i, 3) + 1)\n"
             "    q_n[i<=1100] = sum_(k=1)^1001 max(x_n[k], i)\n"
             "    r_n = sum_(i=1)^1001 sum_(j=1)^2 x_n[i]^j\n}\n")
        set(args --compile cellloops_words.ink words)
        string(CONCAT stdout "cannot compile p: a sum or a product of more than 1000 terms whose term reads i in an exponent, for now\n"
                      "cannot compile q: a sum or a product of more than 1000 terms whose term reads i in a call of max, for now\n"
                      "cannot compile r: a sum or a product of more than 1000 terms whose term reads j in an exponent, for now\n")
        set(exit 1)
        check(compile_cellloops_words)
        set(exit 0)
        set(stdout "")

    endif()
    # Each instance at 32x32 checked for 6 steps, apart from the headers'
    # lines to share the parts' time.
    if((NOT PART OR PART EQUAL 8) AND CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
        set(count 6)
        seqcells(exact exact "exact: 6 steps from 0, against exact values" "exact\\.objective${within0}")
        seqcells(held held "held: 6 steps from 0, against exact values" "held\\.field${within0}")
        seqcells(wave wave "wave: 6 ${inexact}" "wave\\.field${within0}")
        foreach(instance IN ITEMS seen flat)
            seqcells(${instance} ${instance} "${instance}: 6 ${inexact}"
                     "${instance}\\.objective${within0}")
        endforeach()
        seqcells(rod rod "rod: 6 steps from 0, against exact values" "rod\\.x${within0}"
                 "rod\\.s${within0}")
        seqcells(rods rods "rods: 6 ${inexact}" "rods\\.x${within0}" "rods\\.s${within0}")
        foreach(instance IN ITEMS ends endsx)
            set(first "6 steps from 0, against exact values")
            if(instance STREQUAL "endsx")
                set(first "6 ${inexact}")
            endif()
            seqcells(${instance} ${instance} "${instance}: ${first}" "${instance}\\.t${within0}"
                     "${instance}\\.w${within0}")
        endforeach()
        seqcells(gate gate "gate: 6 steps from 0, against exact values" "gate\\.r${within0}"
                 "gate\\.q${within0}" "gate\\.d${within0}")
        seqcells(gatex gatex "gatex: 6 ${inexact}" "gatex\\.r${within0}" "gatex\\.q${within0}"
                 "gatex\\.d${within0}")
        seqcells(pulled pulled "pulled: 6 steps from 0, against exact values" "pulled\\.d${within0}")
    endif()

    if(NOT PART OR PART EQUAL 9)
        # The owner's tank at 64x64: forward was 1,006,007 bytes, nested
        # 2,106,456, and flattened refused.
        foreach(model IN ITEMS forward:8000 nested:10000 flattened:45000)
            string(REPLACE ":" ";" model "${model}")
            list(GET model 1 bytes)
            list(GET model 0 model)
            set(args --compile ${story} ${model} -o ${model}.h)
            check(compile_cellloops_${model})
            strict(compile_cellloops_${model} ${model}.h)
            smaller(compile_cellloops_${model} ${model}.h ${bytes})
        endforeach()
    endif()
    if((NOT PART OR PART EQUAL 9) AND CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
        set(count 4)
        seqcells(tank tank "tank: 4 ${inexact}" "tank\\.field${within0}")
        seqcells(tanked tanked "tanked: 4 ${inexact}" "tanked\\.objective${within0}")
        seqcells(tankflat tankflat "tankflat: 4 ${inexact}" "tankflat\\.objective${within0}")
        # The 32x32 tank in float, here to share the parts' time: 8 steps,
        # Python's floats give 1.7, 2.2 and 96 units at most.
        set(count 8)
        set(float --float)
        set(units "units of a float${about}")
        set(infloat "8 steps from 0 in float, against exact values until 0 and inexact ones from there")
        seqcells(wave wavef "wave: ${infloat}"
                 "wave\\.field: within [0-9.e+-]+, [0-9](\\.[0-9])? ${units}")
        seqcells(seen seenf "seen: ${infloat}"
                 "seen\\.objective: within [0-9.e+-]+, [0-9](\\.[0-9])? ${units}")
        seqcells(flat flatf "flat: ${infloat}"
                 "flat\\.objective: within [0-9.e+-]+, ([0-9](\\.[0-9])?|[1-9][0-9]|1(\\.[0-9])?e\\+02) ${units}")
    endif()

endif()
