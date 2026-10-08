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
                    RESULT_VARIABLE got_exit TIMEOUT 60)
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
set(stdout "inkamath 1.0.0\n")
check(version)

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

# What the compiler cannot express it refuses by name, and writes nothing
# (DESIGN.md, phase 14, step 2).
file(WRITE "${OUT}/model.ink" "a_0 = 1\na_n = a_(n-1) + !n\n")
file(REMOVE "${OUT}/model.h")
set(args --compile model.ink -o model.h)
set(stderr "inkamath: cannot compile a: a factorial\n")
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
file(WRITE "${OUT}/refused.ink" "sq(x) = x^2\na_0 = 1\na_n = a_(n-1) + !n\nb_n = a_n + 1\nc_n = sq(u_n)\ns(g = 1, u_n) = { v_0 = 0\n v_n = g*v_(n-1) + u_n }\nd_n = s(g = n, u_n = c_n).v_n\n")
set(args --compile refused.ink)
set(stdout "cannot compile a: a factorial\ncannot compile b: a, which cannot be compiled\ncannot compile d: an instance of s, which keeps a history, made anew at each step\n")
set(stderr "")
set(exit 1)
check(compile_refusals)

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
set(stdout "")
check(compile_c86)

file(WRITE "${OUT}/logged.ink" "lg(a = 2) = {\n    y_n = log(a + n)\n}\n")
set(args --compile logged.ink lg)
set(stdout "")
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
     "f_n = [n 1;; 2 3 4]\ng_n = lim p(n*T)\nh_n = grad_(V = n*T) sum_(b=1)^2 [1 1]*V[b]*[1; 1]\n"
     "m_n[b<=2, i<=1, j<=1] | n > 2 = b\nm_n[b<=2, i<=1, j<=1] = 0\np(A)_0 = A\n"
     "p(A)_k = p(A)_(k-1)/2\nq_n = [n;; 1] + [1 2]\nr_n = [n;; 1]*[1 2; 3 4]\n"
     "y_0 = 0\ny_n = y_(n-1) + T[2,1,2]\nP[b<=2, j<=2, k<=2] = b\n")
set(args --compile tensor.ink)
set(stdout "cannot compile a: a 2x2x2 tensor takes one index or three, not two
cannot compile b: only a matrix has a power, not a 2x2x2 tensor
cannot compile c: a comparison of matrices
cannot compile d: a 2x2x2 tensor and a 3x2x2 tensor have different numbers of slices
cannot compile f: the slices of a tensor have one size, not 1x2 and 1x3
cannot compile g: a tensor in a limit, for now
cannot compile h: a derivative with respect to a tensor, for now
cannot compile m: a tensor's cells under a guard that is not a constant, for now
cannot compile q: a tensor whose slices are single values met by a matrix, for now
cannot compile r: a tensor whose slices are single values met by a matrix, for now
")
set(exit 1)
check(compile_tensor_refused)

# The prelude's mod refuses a matrix to divide by, as the interpreter does,
# rather than compile the product of matrices it would be.
file(WRITE "${OUT}/mod.ink" "y_n = [1 1]*mod(n, [2 3; 5 11])*[1; 1]\n")
set(args --compile mod.ink)
set(stdout "cannot compile y: mod needs a single value to divide by, not a 2x2 matrix; write it by its cells\n")
set(exit 1)
check(compile_mod_refused)

set(args --compile model.ink)
file(WRITE "${OUT}/model.ink" "a_0 = 1\na_n = a_(n-1) + n\n")
set(stdout "")
set(exit 0)
check(compile_nothing_refused)

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

# An instance checked: the C program its check writes is built and run by the
# tests, beside the compiled models.
set(args --check models.ink loss -o loss.c)
set(stderr "inkamath: models.ink defines no instance loss\n")
set(exit 1)
check(check_no_instance)

set(args --check models.ink gain)
set(stderr "inkamath: --check takes a transcript, or a file, an instance it defines and -o check.c\nTry 'inkamath --help'.\n")
set(exit 2)
check(check_usage)

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

# C153: a power of 1/2 is C's sqrt, rounded correctly, where pow need not be.
file(WRITE "${OUT}/c153.ink" "x_0 = 2\nx_n = x_(n-1)^(1/2) + x_(n-1)^0.5\n")
set(args --compile c153.ink -o c153.h)
check(compile_c153)
holds(compile_c153 c153.h "2.0 : sqrt(0.0 + m_->x[1]) + sqrt(0.0 + m_->x[1]);\n")
set(args --compile c153.ink --float -o c153f.h)
check(compile_float_c153)
holds(compile_float_c153 c153f.h "2.0f : sqrtf(0.0f + m_->x[1]) + sqrtf(0.0f + m_->x[1]);\n")

# C161: a float's inverse starts from the identity in floats, which MSVC's
# /W4 asks of an int converted to one.
file(WRITE "${OUT}/c161.ink" "z_n = [2 1; 1 n]^-1*[1; 1]\n")
set(args --compile c161.ink --float -o c161.h)
check(compile_float_c161)
holds(compile_float_c161 c161.h "; ++j) r[i][j] = i == j ? 1.0f : 0.0f;\n")

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
      [[    m_->y[0] = level_lim0(m_, m_->x[0]);
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
      [[    m_->y[0] = walked_lim0(m_, isnan(m_->x[0]) ? NAN : m_->x[0] > 0.0 ? m_->x[0] : NAN);
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
string(REPLACE "NAN_CLAMPED_H" "PID_CLAMPED_H" named "${named}")
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
set(inputs "${CMAKE_CURRENT_LIST_DIR}/compile/inputs.ink")
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
]] [[static inline void dot_step(dot* m_, const double x[2]) {
    ++m_->index_;
    memcpy(m_->x[0], x, sizeof m_->x[0]);
]])
holds(compile_inputs_ctl ctl.h [[
 * A step takes r_n and s_n (2x1), the inputs at its index. An input of more
 * than one cell is a pointer to its cells, row by row. After a step, m.name[k]
 * is name_(n-k) for each sequence: r, s and u.
]] [[static inline void ctl_step(ctl* m_, double r, const double s[2]) {
    ++m_->index_;
    m_->r[0] = r;
    memcpy(m_->s[0], s, sizeof m_->s[0]);
]])
holds(compile_inputs_turn turn.h [[static inline void turn_init(turn* m_) {
    memset(m_, 0, sizeof *m_);
    m_->x[0][0][0] = 1.0;
    m_->x[0][1][0] = -1.0;
    m_->index_ = -1;
    turn_update(m_);
}
]] [[static inline void turn_step(turn* m_, const double x[2]) {
    ++m_->index_;
    memcpy(m_->x[1], m_->x[0], sizeof m_->x[1]);
    memcpy(m_->x[0], x, sizeof m_->x[0]);
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

# The prelude's charpoly compiles; rho does not yet, by hurwitzb's factorial,
# and grad refuses it and abscissa where A moves, as the interpreter does
# (DESIGN.md, the characteristic polynomial and stability). A 1x1 matrix is
# refused alike, where its sizes went unbound, "n is not defined" (C213).
file(WRITE "${OUT}/charpoly.ink" "x_n = rho([n 1; -1 1/2])\nz_n = charpoly([n 1; 2 3])[2]\ng_n = grad_(a = n) rho([a 1; -1 1/2])\nh_n = grad_(a = n) abscissa(a)\ns_n = rho([1/(n+2)])\n")
set(args --compile charpoly.ink)
set(stdout "cannot compile g: grad cannot differentiate rho yet\ncannot compile h: grad cannot differentiate abscissa yet\ncannot compile s: a factorial\ncannot compile x: a factorial\n")
set(exit 1)
check(compile_charpoly_refused)

# Nor do eig and smax, by a comparison of matrices and hurwitzb's factorial,
# and grad refuses them where A moves (DESIGN.md, eig and smax).
file(WRITE "${OUT}/eig.ink" "x_n = eig([n 1; 1 2])[1]\ny_n = smax([n 2; 3 4])\ng_n = grad_(a = n) eig([a 1; 1 2])[1]\nh_n = grad_(a = n) smax([a 2; 3 4])\n")
set(args --compile eig.ink)
set(stdout "cannot compile g: grad cannot differentiate eig yet\ncannot compile h: grad cannot differentiate smax yet\ncannot compile x: a comparison of matrices\ncannot compile y: a factorial\n")
set(exit 1)
check(compile_eig_refused)

# Nor do hinf and dhinf, by hurwitzb's factorial in their guards, and grad
# refuses them where A moves (DESIGN.md, the H-infinity norm). Of a 1x1
# matrix, dhinf's sizes went unbound, "n is not defined", and hinf's
# sequence with parameters was "in a limit's terms", where there is none
# (C213).
file(WRITE "${OUT}/hinf.ink" "x_t = hinf(-t-1, 1, 1)\ng_t = grad_(a = t) hinf(-a-1, 1, 1)\nh_t = grad_(a = t) dhinf(1/(a+2), 1, 1)\nd_t = dhinf(1/(t+2), 1, 1)\n")
set(args --compile hinf.ink)
set(stdout "cannot compile d: a factorial\ncannot compile g: grad cannot differentiate hinf yet\ncannot compile h: grad cannot differentiate dhinf yet\ncannot compile x: a factorial\n")
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
set(stderr "inkamath: cannot compile inner.c: inner.c_0 reads inner.x_-1, before the stream, where inner.x has no history\n")
set(exit 1)
check(history_gained)

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


# A float target (DESIGN.md, compile/float.ink): beside --compile or an
# instance's --check, floats where the header writes doubles.
set(args --float first.txt)
set(stderr "inkamath: --float takes --compile, or --check with an instance\nTry 'inkamath --help'.\n")
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
]] [[static inline void dot_step(dot* m_, const float x[2]) {
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
      [[static inline float gate_lim0(const gate* m_, float arg_z) {]]
      [[        const float t_ = t1_ + (t1_ + (0.0f - t2_)) * arg_z / (float)k_;
        if (started_) {
            step_ = fabsf(t_ - t1_);
            if (step_ <= 2 * FLT_EPSILON * fabsf(t_) && isfinite(t_) && stepped_) return t_;
            if (step_ <= 1e-10f && stepped_ &&
]] [[    m_->p[0][0][0] = 1.0f / (1.0f + gate_lim0(m_, 0.0f + (0.0f - (0.0f * m_->w[0][0][0] + 0.0f * m_->w[0][1][0] + m_->b[0]))));
]])

set(args --check ${CMAKE_CURRENT_LIST_DIR}/compile/drift.ink calm --float -o float/calm.c)
check(float_calm)
holds(float_calm float/calm.c [[static const float in_0[100] = {
    0.0f, 0.1f, 0.2f, 0.3f,
]] [[static float got_0[100];
]] [[        calm_step(&m, in_0[n]);
        memcpy(&got_0[n * 1], &m.v[0], sizeof(float) * 1);
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

# C104: a header or a program named after a word C keeps, or after a standard
# header it includes, is refused.
set(args --compile ${inputs} dot --float -o float.h)
set(stderr "inkamath: 'float' is a name C keeps, and the header is named after it\n")
set(exit 2)
check(compile_c104)
set(args --check ${CMAKE_CURRENT_LIST_DIR}/compile/drift.ink calm -o math.c)
set(stderr "inkamath: 'math' is a name C keeps, and the program is named after it\n")
set(exit 2)
check(check_c104)

# grad compiled (DESIGN.md, compile/grad.ink): the gradient written out, each
# function of the prelude called on a value that moves with one for its part,
# and what stays refused named in the interpreter's words where it has them.
set(grad "${CMAKE_CURRENT_LIST_DIR}/compile/grad.ink")
set(args --compile ${grad} lsq -o lsq.h)
check(compile_grad_line)
holds(compile_grad_line lsq.h [[    m_->w[0][0][0] = m_->index_ == 0 ? 0.0 : m_->w[1][0][0] + (0.0 - m_->eta * (2.0 * pow(1.0 * m_->w[1][0][0] + 0.0 * m_->w[1][1][0] - 1.0, 1.0) * 1.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 1.0 * m_->w[1][1][0] - 3.0, 1.0) * 1.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 2.0 * m_->w[1][1][0] - 5.0, 1.0) * 1.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 3.0 * m_->w[1][1][0] - 7.0, 1.0) * 1.0 / 8.0));
    m_->w[0][1][0] = m_->index_ == 0 ? 0.0 : m_->w[1][1][0] + (0.0 - m_->eta * (2.0 * pow(1.0 * m_->w[1][0][0] + 0.0 * m_->w[1][1][0] - 1.0, 1.0) * 0.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 1.0 * m_->w[1][1][0] - 3.0, 1.0) * 1.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 2.0 * m_->w[1][1][0] - 5.0, 1.0) * 2.0 / 8.0 + 2.0 * pow(1.0 * m_->w[1][0][0] + 3.0 * m_->w[1][1][0] - 7.0, 1.0) * 3.0 / 8.0));
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
file(WRITE "${OUT}/grad_refused.ink" "a_n = grad_(t = x_n) grad_(s = t) s^3\nb_n = grad_(t = x_n) lim p(t)\nc_n = grad_(v = [x_n; 1]) 2*v\nd_n = grad_(t = x_n) 2^t\nf_n = grad_(t = x_n) t^x_n\ng_n = grad_(t = x_n) [1 1]*[t 1; 0 t]^2*[1; 1]\nh_n = grad_(t = x_n) 5\nk_n = grad_(t = x_n) sq\nm_n = grad_(t = x_n) amp(k = t).y\namp(k = 1) = {\n    y = 2*k\n}\np(r)_0 = 1\np(r)_k = r*p(r)_(k-1)/4 + 1\nsq = t^2\nt = 3\n")
set(args --compile grad_refused.ink)
set(stdout "cannot compile a: a derivative of a derivative, for now\ncannot compile b: a derivative of a limit, for now\ncannot compile c: grad of a matrix with respect to a matrix is a Jacobian, which it does not give\ncannot compile d: grad cannot differentiate a power whose exponent changes with t, unless its base is e\ncannot compile f: a derivative of a power whose exponent is not a constant, for now\ncannot compile g: a derivative of a matrix power, for now\ncannot compile h: grad's expression does not read t\ncannot compile k: sq reads the global t, which grad's t does not reach\ncannot compile m: grad cannot differentiate through an instance yet\n")
set(exit 1)
check(compile_grad_refused)

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
holds(compile_c133s c133s.h "    m_->w[0] = (isnan(t0_) ? NAN : isnan((isnan(m_->x[0]) ? NAN : m_->x[0] == 2.0 ? NAN : m_->x[0] > 2.0 ? 1.0 : 0.0) * m_->x[0]) ? NAN : (isnan(m_->x[0]) ? NAN : m_->x[0] == 2.0 ? NAN : m_->x[0] > 2.0 ? 1.0 : 0.0) * m_->x[0] > -1.0 ? 1.0 : 0.0);\n")

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
holds(compile_c136g c136g.h "    const double t1_ = (isnan(t0_) ? NAN : 2.0);\n    m_->u[0] = (isnan(t1_) ? NAN : 0.0);\n")

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
 * assigning one, call mha_update.
 */
]=] [=[typedef struct mha {
    double d;
    long long index_;
    double X[1][2][3][4];
    double O[1][2][3][4];
} mha;
]=] [=[static inline void mha_step(mha* m_, const double X[24]) {
    ++m_->index_;
    memcpy(m_->X[0], X, sizeof m_->X[0]);
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
]=] [=[static inline void ring_init(ring* m_) {
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
]=] [=[static inline void ring_step(ring* m_, const double X[4]) {
    ++m_->index_;
    memcpy(m_->X[1], m_->X[0], sizeof m_->X[1]);
    memcpy(m_->s[1], m_->s[0], sizeof m_->s[1]);
    memcpy(m_->X[0], X, sizeof m_->X[0]);
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
]=] [=[static inline void ring_step(ring* m_, const float X[4]) {
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
]=] [=[static inline void sgd_step(sgd* m_, const double X[8], const double Y[4]) {
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
    const double t128_ = pow(t127_, 2.0);
]] [[    m_->h[0] = pow(t254_, 2.0);
]])
set(args --compile ${iterates} mpc -o mpc.h)
check(compile_iterates_mpc)
holds(compile_iterates_mpc mpc.h " call mpc_update. Compiled in as constants, these cannot change: iters.\n")
file(WRITE "${OUT}/iterates.ink" "a_n = r(x_n)_n\nb_n = r(x_n)_1001\nc_n = q(x_n)_3\nd_n = z(x_n)_2\ne_n = p(x_n)_2\nf_n = w(x_n)_70\ng_n = r(x_n)_(1/2)\nh_n = lim nw(x_n)\nu_n = y(x_n)_0\nnw(a)_0 = a\nnw(a)_k = nw(a)_(k-1)/2 + r(a)_2\np(x)_0 = x\np(x)_k = p(x)_(k+1)/2\nq(x)_0 = x\nq(x)_k = q(x)_(k-2) + 1\nr(x)_0 = x\nr(x)_k = r(x)_(k-1)/2 + 1\nw(x)_0 = x\nw(x)_k = w(x/2)_(k-1)\ny(x)_0 = [x; 1]\ny(x)_k[j<=2] = x*j\nz(x)_0 = x\nz(x)_k = z(x)_k/2\n")
set(args --compile iterates.ink)
set(stdout "cannot compile a: a sequence with parameters read at an index that is not a constant
cannot compile b: r_1001 is 1001 terms from its base, and a step writes out at most 1000
cannot compile c: q has no clause for index -1
cannot compile d: z is defined by itself
cannot compile e: p_(...): a term after the one being computed
cannot compile f: calls nested 64 deep, which a recursion its guards do not end would pass
cannot compile g: an index must be a whole number, not 0.5
cannot compile h: a sequence with parameters in a limit's terms, for now
cannot compile u: a sequence with parameters by cells, for now
")
set(exit 1)
check(compile_iterates_refused)

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
cannot compile d: a matrix inverse inside a limit's terms
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
