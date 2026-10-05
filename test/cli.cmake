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

# A tensor is refused by name: the interpreter is the reference it is held to
# first (DESIGN.md, tensors of rank 3).
file(WRITE "${OUT}/tensor.ink" "T = [1 2;; 3 4]\ny_0 = 0\ny_n = y_(n-1) + T[2,1,2]\nP[b<=2, j<=2, k<=2] = b\n")
set(args --compile tensor.ink)
set(stdout "cannot compile P: a tensor\ncannot compile y: a tensor\n")
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
# one cell (C83).
file(WRITE "${OUT}/wide.ink" "mm(x_n) = {\n    y_n = [1 2]*x_n\n}\nv = mm(x_n = [n; 1])\n")
set(args --check wide.ink v -o wide.c)
set(stderr "inkamath: v.x_(0) has 2 cells, where the compiled step takes a single value\n")
set(exit 1)
check(check_matrix_input)

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
holds(compile_inputs_avg avg.h [[ Compiled in, as a size, a bound or a
 * lag cannot change: d.
]])

# Refused: a history of another size than the input, a tensor, a default and
# an instance within a model of another size than the model states, and a
# size that reads the index, which the interpreter refuses as it reads it.
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
unsized(batch "batch(x_n[b<=2, j<=1, k<=2]) = {\n    y_n = x_n[2]*[1; 1]\n}\n"
        "cannot compile x: a tensor")
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
refused(short "x_n) = {\n    x_(-1) = 0\n    c_n = x_(n-2)"
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

