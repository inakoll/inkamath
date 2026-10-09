# --steps n on the command line (DESIGN.md, next in line), in the form of
# test/cli.cmake's cases and run by its 'check' and 'holds'. Unwired, as
# every case fails today on "unknown option '--steps'": the implementation
# moves them into cli.cmake, 'help_steps' in place of 'help', and deletes
# this file. The reports of steps.ink are its programs', built by
# test/CMakeLists.txt.

file(WRITE "${OUT}/coast.ink" "fall(N = 1000, T = 125, g = 1, v0 = 64) = {\n    h = T/N\n    v_0 = v0\n    v_n = v_(n-1) - g*h\n    y_0 = 0\n    y_n = y_(n-1) + h*v_n\n}\ncoast = fall()\n")

# The program steps the instance n times and says so first, anywhere the
# option is given on the line.
set(args --check coast.ink coast --steps 1001 -o coast.c)
check(check_steps)
holds(check_steps coast.c
    [[printf("coast: 1001 steps from 0, against exact values\n");]]
    "    for (int n = 0; n < 1001; ++n) {\n"
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
set(stderr "inkamath: coast.ink defines no instance nothing\n")
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
if(EXISTS "${OUT}/refused.c")
    message(SEND_ERROR "check_steps_refused: refused.c was written")
endif()

# Given twice, even alike.
set(args --check coast.ink coast --steps 10 --steps 10 -o twice.c)
set(stderr "inkamath: --steps is given twice\nTry 'inkamath --help'.\n")
set(exit 2)
check(check_steps_twice)

# Only an instance's check has steps: not a header, a transcript replayed,
# or files run.
file(WRITE "${OUT}/said.ink" ">> 1+1\n2\n")
foreach(case IN ITEMS "compile;--compile;coast.ink;fall;-o;fall.h"
                      "transcript;--check;said.ink"
                      "run;coast.ink")
    list(POP_FRONT case name)
    set(args ${case} --steps 10)
    set(stderr "inkamath: --steps takes --check with an instance\nTry 'inkamath --help'.\n")
    set(exit 2)
    check(check_steps_${name}_refused)
endforeach()
# An instance with no -o is told what --check takes, as without --steps.
set(args --check coast.ink coast --steps 10)
set(stderr "inkamath: --check takes a transcript, or a file, an instance it defines and -o check.c\nTry 'inkamath --help'.\n")
set(exit 2)
check(check_steps_usage)

set(args --help)
set(stdout [=[
Usage: inkamath [options] [file...]
       inkamath --compile file [model] [-o header.h]
       inkamath --check file instance -o check.c
       inkamath --check transcript.ink

Runs the files in order and exits; with no file, reads standard input.
At a terminal the prompt edits the line and keeps its history.

  -i          read standard input after the files
  --echo      print each input before its answer, as a transcript
  --version   print the version and exit
  --compile   write the sequences the file defines as a C header over
              doubles, named after the header: a struct, an init and a step;
              given a model the file defines, those of the model instead;
              without -o, list what would not compile, and write nothing
  --check     write a C program that steps an instance the file defines,
              compiled, on the inputs the interpreter gives it, holds each
              term to the interpreter's exact one and says where one drifts;
              given a transcript alone, replay it and report each answer
              that is not the one recorded
  --float     with --compile or --check, write floats where they write
              doubles
  --steps n   with --check and an instance, step it n times rather than
              100, n from 1 to 100000
  --help      print this and exit

A file whose first line that is not blank or a comment starts with '>>'
is a transcript, and only its '>>' lines are read.
]=])
check(help_steps)
