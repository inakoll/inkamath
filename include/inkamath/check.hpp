#ifndef INKAMATH_CHECK_HPP
#define INKAMATH_CHECK_HPP

#include "inkamath/compile.hpp"
#include "inkamath/interpreter.hpp"
#include "inkamath/number.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

// The oracle (DESIGN.md, next in line): a C program that steps an instance,
// compiled, on the inputs the interpreter gives it, and holds each term to the
// interpreter's, which is exact where the mathematics allows. Inputs are
// replayed rather than computed, so a difference is the step's own. A guard
// that takes another clause compiled is reported first, with its margin.
class CheckC {
public:
    using Value = Matrix<Number>;

    static constexpr int steps = 100;

    using Definition = std::shared_ptr<const Reference<Value>>;

    // 'definition' names the instance, and 'unfed' is it with its parameters
    // and no input, as ReferenceStack::Unfed makes it.
    static std::string Program(Interpreter<Number>& session, const std::string& instance,
                               const Definition& definition, const std::string& module,
                               const std::string& source, const Scope<Value>& unfed) {
        ReferenceStack<Value>&   stack = session.Definitions();
        // In float, a thousandth: three digits asked of seven (DESIGN.md).
        const bool        floats = CompileC::floats;
        const std::string tol = floats ? "1e-3" : "1e-9", form = floats ? "%.9g" : "%.17g",
                          got = floats ? "(double)got[k]" : "got[k]";
        const CompileC::Compiled compiled =
            CompileC::Build(stack, module, source, unfed.model, &unfed, true);
        // Every guard asked on the way, from the first term on, by run, term
        // and cell: a term asked for again is remembered, and its guards are
        // not asked twice. A function's by run, ask and call, a call begun
        // where its guards are tried again; none while a margin is measured,
        // as a disturbed run measures none (DESIGN.md).
        std::map<std::tuple<int, const Reference<Value>*, int, int, int>, Asked> asked;
        using Sequence = CompileC::Compiled::Sequence;
        std::map<std::tuple<int, const Sequence*, int, const Reference<Value>*>, std::vector<Asked>>
                                        calls;
        int                             run = 0;
        std::pair<const Sequence*, int> asking{nullptr, 0};
        bool                            measuring = false;
        const auto hook = [&](const Reference<Value>& reference, const Clause<Value>& clause, int n,
                              int row, int col, bool held, EvaluationVisitor<Value>& evaluator) {
            if (measuring) return;
            const int place = static_cast<int>(&clause - reference.Clauses().data());
            auto*     call  = clause.parameters.general()
                                  ? nullptr
                                  : &calls[{run, asking.first, asking.second, &reference}];
            if (call && (call->empty() || call->back().chosen ||
                         call->back().margins.rbegin()->first >= place))
                call->emplace_back();
            Asked&              seen = call ? call->back() : asked[{run, &reference, n, row, col}];
            const Setting<bool> quiet(measuring, true);
            seen.margins[place] =
                run ? std::optional<Number>() : Margin(*clause.parameters.guard(), evaluator);
            if (held) seen.chosen = place;
        };
        const Setting<decltype(stack.guards)> listening(stack.guards, hook);
        // A term the file asked before was heard by no guard (C106).
        stack.Forget();
        const int         first = compiled.first;
        std::string       data, stepped, held;
        for (std::size_t k = 0; k < compiled.inputs.size(); ++k) {
            std::vector<std::string> values;
            for (int n = first; n < first + steps; ++n) {
                const Term input = At(session, instance + "." + compiled.inputs[k], n);
                if (!input.error.empty())
                    throw std::runtime_error(instance + "." + compiled.inputs[k] + "_(" +
                                             std::to_string(n) + "): " + input.error);
                if (input.cells.size() != compiled.cells[k])
                    throw std::runtime_error(instance + "." + compiled.inputs[k] + "_(" +
                                             std::to_string(n) + ") has " +
                                             std::to_string(input.cells.size()) +
                                             " cells, where the compiled step takes a single "
                                             "value" +
                                             Unstated(*unfed.model, compiled.inputs[k], input));
                for (const double x : input.reals)
                    values.push_back(!floats           ? Double(x)
                                     : std::signbit(x) ? "-" + CompileC::Double(-x)
                                                       : CompileC::Double(x));
            }
            data += Array("const " + CompileC::Real(), "in_" + std::to_string(k),
                          steps * compiled.cells[k], values);
        }
        // A term of an instance the model writes unnamed is asked of one made
        // again where the instance checked has it, as written (C84): the
        // interpreter cannot name it.
        std::map<std::string, std::shared_ptr<const Scope<Value>>> made;
        for (const auto& [label, kept] : compiled.unnamed) {
            const Scope<Value>* written = stack.InstanceScope(definition);
            for (std::size_t at = 0, point; (point = label.find('.', at)) != std::string::npos;
                 at             = point + 1) {
                const auto outer = made.find(label.substr(0, point));
                written =
                    outer != made.end()
                        ? outer->second.get()
                        : stack.InstanceScope(written->names.at(label.substr(at, point - at)));
            }
            made[label] = stack.Detached(*kept.model, *kept.call, *written, instance + "." + label,
                                         kept.captured);
        }
        const auto ask = [&](const CompileC::Compiled::Sequence& sequence, int index) {
            if (!sequence.unnamed) return At(session, instance + "." + sequence.name, index);
            const std::size_t                            dot = sequence.name.rfind('.');
            const typename ReferenceStack<Value>::Within within(
                stack, made.at(sequence.name.substr(0, dot)).get());
            return At(session, sequence.name.substr(dot + 1), index);
        };
        // The interpreter's own error, estimated (DESIGN.md): each term asked
        // three times more, apart, every rounding disturbed and every limit
        // moved by its remainder, up, down, then either way.
        std::map<std::pair<std::string, int>, std::vector<std::optional<Value>>> again;
        for (std::uint64_t seed = 1; seed <= 3; ++seed) {
            Number::Disturbance disturbance{seed, seed == 1 ? 1 : seed == 2 ? -1 : 0};
            stack.Apart([&] {
                const Setting<Number::Disturbance*> disturbing(Number::disturbed, &disturbance);
                const Setting<decltype(stack.guards)> heard(stack.guards, hook);
                const Setting<int>                    counted(run, static_cast<int>(seed));
                for (const auto& sequence : compiled.sequences) {
                    for (int n = first; n < first + steps; ++n) {
                        if (n < sequence.start && sequence.period > 1) continue;
                        asking           = {&sequence, n};
                        const int  index = Floor(n - sequence.phase, sequence.period);
                        const Term term  = ask(sequence, index);
                        again[{instance + "." + sequence.name, index}].push_back(
                            term.error.empty() ? std::optional(term.value) : std::nullopt);
                    }
                }
            });
        }
        std::optional<int> inexact;  // where the interpreter's terms stop being exact
        // Where a term is a tensor, a cell is named by its slice, its row and its column.
        const bool tensors     = std::any_of(compiled.sequences.begin(), compiled.sequences.end(),
                                             [](const auto& s) { return s.size.slices > 0; });
        const std::string rows = tensors ? "int rows, " : "";
        for (std::size_t k = 0; k < compiled.sequences.size(); ++k) {
            const auto&              sequence = compiled.sequences[k];
            const std::size_t        cells    = sequence.size.count();
            const std::string        id = std::to_string(k), size = std::to_string(steps * cells);
            std::vector<std::string> want, known, why;
            std::vector<double>      about(steps * cells);
            for (int n = first; n < first + steps; ++n) {
                // Before its start the step has no term, and the interpreter
                // should have none either; at another rate it is not asked.
                const bool        before = n < sequence.start;
                const std::string name   = instance + "." + sequence.name;
                const int         index  = Floor(n - sequence.phase, sequence.period);
                asking                   = {&sequence, n};
                const Term        term   = before && sequence.period > 1
                                               ? Term{{}, true, "not asked", {}, {}, {}, false}
                                               : ask(sequence, index);
                if (!term.exact && (!inexact || n < *inexact)) inexact = n;
                const bool given = term.error.empty();
                // Another shape than the step's is the compiler's mistake (C137).
                if (given && term.value.Size() != sequence.size)
                    throw std::runtime_error(name + "_(" + std::to_string(index) + ") is a " +
                                             term.value.Size().Described() +
                                             ", where the compiled step's is a " +
                                             sequence.size.Described());
                double*    at    = &about[static_cast<std::size_t>(n - first) * cells];
                if (given)
                    for (const auto& run : again[{name, index}]) Farther(term.value, run, at);
                // Each cell by itself: one no double holds parts alone (C128).
                for (std::size_t c = 0; c < cells; ++c) {
                    const char kind = !given && term.spent   ? '5'
                                      : !given               ? (before ? '0' : '2')
                                      : !term.odd[c].empty() ? '4'
                                      : before               ? '3'
                                                             : '1';
                    why.push_back(kind == '2'   ? Quoted(term.error)
                                  : kind == '4' ? Quoted(term.odd[c])
                                                : "0");
                    want.push_back(kind == '1' || kind == '3' ? term.cells[c] : "0.0");
                    known.push_back(std::string(1, kind));
                    if (kind == '4') at[c] = 0;
                }
            }
            const bool told = std::any_of(known.begin(), known.end(), [](const std::string& each) {
                return each == "2" || each == "4";
            });
            if (told) data += Array("const char* const", "why_" + id, steps * cells, why);
            const bool estimated = *std::max_element(about.begin(), about.end()) > 0;
            if (estimated) {
                std::vector<std::string> written;
                for (const double e : about) written.push_back(Double(e));
                data += Array("const double", "about_" + id, steps * cells, written);
            }
            data += Array("const double", "want_" + id, steps * cells, want);
            data += Array("const unsigned char", "known_" + id, steps * cells, known);
            data += "static " + CompileC::Real() + " got_" + id + "[" + size + "];\n";
            stepped += "        memcpy(&got_" + id + "[n * " + std::to_string(cells) + "], &m." +
                       sequence.name + "[0], sizeof(" + CompileC::Real() + ") * " +
                       std::to_string(cells) + ");\n";
            held += "    held &= hold_(\"" + instance + "." + sequence.name + "\", " +
                    std::to_string(sequence.size.cols) + ", " +
                    (tensors ? std::to_string(sequence.size.rows) + ", " : "") +
                    std::to_string(cells) + ", " + std::to_string(sequence.start) + ", got_" + id +
                    ", want_" + id + ", known_" + id + ", " +
                    (told ? "why_" + id : std::string("0")) + ", " +
                    (estimated ? "about_" + id : std::string("0")) + ");\n";
        }
        std::string arguments;
        for (std::size_t k = 0; k < compiled.inputs.size(); ++k)
            arguments += compiled.cells[k] == 1 ? ", in_" + std::to_string(k) + "[n]"
                                                : ", &in_" + std::to_string(k) + "[n * " +
                                                      std::to_string(compiled.cells[k]) + "]";
        // The first step at which a disturbed run takes another clause than
        // the interpreter (DESIGN.md), in a flip's words: at one step, the
        // first sequence asked's, and the first run's.
        std::map<std::pair<int, const Sequence*>, std::string> straddles;
        const auto straddle = [&](int n, const Sequence* sequence, const std::string& place,
                                  const std::vector<Clause<Value>>& clauses, int took, int chose,
                                  const Asked& seen) {
            if (took == chose || std::min(took, chose) < 0 ||
                std::max(took, chose) >= static_cast<int>(clauses.size()))
                return;
            const bool earlier = clauses[took].parameters.guarded() &&
                                 (took < chose || !clauses[chose].parameters.guarded());
            std::string line = instance + "." + sequence->name + place + ": at " +
                               std::to_string(n) + " a disturbed run takes '" +
                               clauses[took].written + "' and the interpreter '" +
                               clauses[chose].written + "'";
            const auto margin = seen.margins.find(earlier ? took : chose);
            if (margin != seen.margins.end() && margin->second) {
                const double d = margin->second->Inexact().real();
                char         text[40];
                std::snprintf(text, sizeof text, "%.2g from", d);
                line += std::string("; the guard of the ") + (earlier ? "first" : "second") +
                        " is " + (d == 0.0 ? "exactly on" : text) + " its threshold";
            }
            straddles.emplace(std::pair(n, sequence), line);
        };
        std::string table;
        for (std::size_t k = 0; k < compiled.guarded.size(); ++k) {
            const std::string& name = compiled.guarded[k];
            const auto found = std::find_if(compiled.sequences.begin(), compiled.sequences.end(),
                                            [&](const auto& s) { return s.name == name; });
            const std::size_t       dot = name.rfind('.');
            const Reference<Value>& reference =
                found->unnamed ? *made.at(name.substr(0, dot))->names.at(name.substr(dot + 1))
                               : Resolve(stack, definition, name);
            const auto&              clauses   = reference.Clauses();
            const std::string        id = std::to_string(k), count = std::to_string(clauses.size());
            // A term chosen cell by cell is reported cell by cell; the clause
            // taken where no guard held is the one for every cell, or every term.
            const bool cellwise =
                std::any_of(clauses.begin(), clauses.end(), [](const Clause<Value>& c) {
                    return c.parameters.general() && c.parameters.cells() && c.parameters.guarded();
                });
            int                      general = -1;
            std::vector<std::string> written{"\"\""}, rank{"0"};
            for (std::size_t c = 0, tried = 0; c < clauses.size(); ++c) {
                const ParametersDefinition<Value>& p = clauses[c].parameters;
                if (p.general() && !p.guarded() && p.cells() == cellwise &&
                    (!cellwise || !p.row_name().empty()) && general < 0)
                    general = static_cast<int>(c);
                written.push_back(Quoted(clauses[c].written));
                rank.push_back(
                    std::to_string(p.general() && p.guarded() ? tried++ : clauses.size()));
            }
            data += Array("const char* const", "written_" + id, clauses.size() + 1, written);
            data += Array("const int", "rank_" + id, clauses.size() + 1, rank);
            // At another rate, the clause of the latest term computed, none before.
            const auto term = [&](int n) {
                return found->period > 1 && n < found->start
                           ? std::numeric_limits<int>::min()
                           : Floor(n - found->phase, found->period);
            };
            const std::size_t cols   = found->size.cols;
            const std::size_t places = cellwise ? found->size.count() : 1;
            for (std::size_t c = 0; c < places; ++c) {
                const int                row = cellwise ? static_cast<int>(c / cols) + 1 : 0;
                const int                col = cellwise ? static_cast<int>(c % cols) + 1 : 0;
                const std::string        at  = id + (cellwise ? "_" + std::to_string(c) : "");
                const std::string        place =
                    cellwise ? "[" + std::to_string(row) + "," + std::to_string(col) + "]" : "";
                std::vector<std::string> want, margin;
                for (int n = first; n < first + steps; ++n) {
                    const auto asking = asked.find({0, &reference, term(n), row, col});
                    for (int r = 1; r <= 3 && asking != asked.end(); ++r)
                        if (const auto them = asked.find({r, &reference, term(n), row, col});
                            them != asked.end())
                            straddle(n, &*found, place, clauses,
                                     them->second.chosen.value_or(general),
                                     asking->second.chosen.value_or(general), asking->second);
                    want.push_back(asking == asked.end() ? "0"
                                   : asking->second.chosen
                                       ? std::to_string(*asking->second.chosen + 1)
                                       : std::to_string(general + 1));
                    for (std::size_t p = 0; p < clauses.size(); ++p) {
                        std::optional<Number> distance;
                        if (asking != asked.end() &&
                            asking->second.margins.count(static_cast<int>(p)))
                            distance = asking->second.margins.at(static_cast<int>(p));
                        margin.push_back(distance ? Double(distance->Inexact().real()) : "-1.0");
                    }
                }
                data += Array("const int", "clause_" + at, steps, want);
                data += Array("const double", "margin_" + at, steps * clauses.size(), margin);
                data += "static int taken_" + at + "[" + std::to_string(steps) + "];\n";
                stepped += "        taken_" + at + "[n] = m." + name + "_clause_" +
                           (cellwise ? "[" + std::to_string(c) + "]" : "") + ";\n";
                table += "        {\"" + instance + "." + name + place + "\", " + count +
                         ", written_" + id + ", rank_" + id + ", clause_" + at + ", taken_" + at +
                         ", margin_" + at + "},\n";
            }
        }
        // A function's, named by the sequence asked, its calls paired in order.
        for (const auto& [key, heard] : calls) {
            const auto& [r, sequence, n, reference] = key;
            if (r || !sequence) continue;
            const auto& clauses  = reference->Clauses();
            const int   fallback = static_cast<int>(
                std::find_if(clauses.begin(), clauses.end(),
                               [](const auto& c) { return !c.parameters.guarded(); }) -
                clauses.begin());
            for (std::size_t c = 0; c < heard.size(); ++c)
                for (int other = 1; other <= 3; ++other)
                    if (const auto them = calls.find({other, sequence, n, reference});
                        them != calls.end() && c < them->second.size())
                        straddle(n, sequence, "", clauses,
                                 them->second[c].chosen.value_or(fallback),
                                 heard[c].chosen.value_or(fallback), heard[c]);
        }
        const std::string against = !inexact ? "exact values"
                                             : "exact values until " + std::to_string(*inexact) +
                                                   " and inexact ones from there";

        std::string out;
        out += "/* Generated by 'inkamath --check' from " + source + ": the instance\n";
        out += " * compiled, stepped on the inputs the interpreter gives it, and each term\n";
        out += " * held to the interpreter's. Build it as the header would be built.\n";
        out += " * Beside each, how far the interpreter's own terms may be from the exact\n";
        out += " * ones: each asked three times more, every rounding taken the other way\n";
        out += " * with probability one half and every limit moved by the remainder it\n";
        out += " * estimated, and the farthest kept. An estimate, not a bound. */\n";
        out += compiled.header + "\n#include <stdio.h>\n\n";
        out += "/* A term parts from the interpreter's where they differ by more than a\n";
        out += std::string(" * ") + (floats ? "thousandth" : "billionth") +
               " of one plus the interpreter's term; where the interpreter\n";
        out += " * gives none, from the sequence's start, unless it is NaN, and before it,\n";
        out += " * where the interpreter gives one. 'known' says which: 0 not asked, 1 a\n";
        out += " * term, 2 none, 3 one before the start, 4 one that is no finite double,\n";
        out += " * 5 none, the interpreter out of steps;\n";
        out += " * 'about' is each term's estimate, or none where every one is 0. */\n";
        out += "static int hold_(const char* name, int cols, " + rows + "int cells, int from,\n";
        out += "                 const " + CompileC::Real() + "* got, const double* want,\n";
        out += "                 const unsigned char* known, const char* const* why,\n";
        out += "                 const double* about) {\n";
        out += "    double worst = 0.0, most = 0.0" + std::string(floats ? ", units = 0.0" : "") +
               ";\n";
        out += "    int    past  = -1, compared = 0, spent = -1, more = 0;\n";
        out += "    for (int k = 0; k < " + std::to_string(steps) + " * cells; ++k) {\n";
        out += "        const double difference = fabs(" + got + " - want[k]);\n";
        out += "        const int    n = " + std::to_string(first) + " + k / cells;\n";
        out += "        const double e = about ? about[k] : 0.0;\n";
        out += "        if (known[k] == 0 || (known[k] == 2 && isnan(" + got + "))) continue;\n";
        out += "        if (known[k] == 5) {\n            more += spent >= 0;\n";
        out += "            if (spent < 0) spent = n;\n            continue;\n        }\n";
        out +=
            "        if (known[k] == 1 && difference <= " + tol + " * (1.0 + fabs(want[k]))) {\n";
        out += "            ++compared;\n";
        out += "            if (difference > worst) worst = difference;\n";
        // In units of a float at the interpreter's term, or at 1 below it.
        if (floats)
            out +=
                "            units = fmax(units, ldexp(difference, 23 - ilogb(fmax(fabs(want[k]), "
                "1.0))));\n";
        out += "            if (e > most) most = e;\n";
        out += "            if (past < 0 && e > " + tol + " * (1.0 + fabs(want[k]))) past = n;\n";
        out += "            continue;\n        }\n";
        out += "        printf(\"%s\", name);\n";
        if (tensors)
            out +=
                "        if (cells > rows * cols)\n            printf(\"[%d,%d,%d]\", k % cells / "
                "(rows * cols) + 1, k % (rows * cols) / cols + 1, k % cols + 1);\n        else ";
        out += std::string(tensors ? "" : "        ") + "if (cells > 1)\n";
        out += "            printf(\"[%d,%d]\", k % cells / cols + 1, k % cells % cols + 1);\n";
        out += "        if (known[k] == 3)\n";
        out += "            printf(\": none at %d, where the interpreter gives %.17g\", n,\n";
        out += "                   want[k]);\n";
        out += "        else if (known[k] == 2)\n";
        out +=
            "            printf(\": " + form + " at %d, where the interpreter gives none: %s\",\n";
        out += "                   " + got + ", n, why[k]);\n";
        out += "        else if (known[k] == 4)\n";
        out += "            printf(\": " + form + " at %d, where the interpreter's term is %s\",\n";
        out += "                   " + got + ", n, why[k]);\n";
        out += "        else\n";
        out += "            printf(\": " + form + " at %d, where the interpreter gives %.17g\",\n";
        out += "                   " + got + ", n, want[k]);\n";
        out += "        if (e > 0.0)\n";
        out +=
            "            printf(\"; the interpreter's term about %.2g from the exact one\", e);\n";
        out += "        printf(\"\\n\");\n";
        out += "        return 0;\n    }\n";
        // A term the interpreter gave up on is no answer the step could be held to.
        const std::string spent =
            "    if (spent >= 0) {\n"
            "        printf(\"; the interpreter ran out of steps at %d\", spent);\n"
            "        if (more) printf(\" and %d more\", more);\n"
            "        printf(\", not compared\");\n    }\n";
        // Where neither side gives a value, 'within 0' would say one was held.
        out += "    if (!compared) {\n";
        out += "        printf(\"%s: no value compared\", name);\n";
        out += "        for (int k = 0; k < " + std::to_string(steps) + " * cells; ++k)\n";
        out += "            if (known[k] == 2) {\n";
        out +=
            "                printf(\", the step NaN where the interpreter gives none, as at "
            "%d: %s\", " +
            std::to_string(first) + " + k / cells, why[k]);\n";
        out += "                break;\n            }\n";
        out += spent + "        printf(\"\\n\");\n        return 1;\n    }\n";
        out += floats
                   ? "    printf(\"%s: within %.2g, %.2g units of a float\", name, worst, units);\n"
                   : "    printf(\"%s: within %.2g\", name, worst);\n";
        out += "    if (from > " + std::to_string(first) + ") printf(\", from %d\", from);\n";
        out += "    if (most > 0.0)\n";
        out +=
            "        printf(\"; the interpreter's terms about %.2g from the exact ones\", most);\n";
        out += "    if (past >= 0) printf(\", past the tolerance from %d\", past);\n";
        out += spent + "    printf(\"\\n\");\n    return 1;\n}\n\n";
        if (!table.empty()) out += Flips(first);
        out += data + "\nint main(void) {\n";
        out += "    " + module + " m;\n    int held = 1;\n    " + module + "_init(&m);\n";
        out += "    for (int n = 0; n < " + std::to_string(steps) + "; ++n) {\n";
        out += "        " + module + "_step(&m" + arguments + ");\n" + stepped + "    }\n";
        out += "    printf(\"" + instance + ": " + std::to_string(steps) + " steps from " +
               std::to_string(first) + (floats ? " in float" : "") + ", against " + against +
               "\\n\");\n";
        if (!straddles.empty()) out += "    puts(" + Quoted(straddles.begin()->second) + ");\n";
        if (!table.empty())
            out += "    static const guarded_ guarded[] = {\n" + table + "    };\n" +
                   "    held &= flips_(guarded, (int)(sizeof guarded / sizeof guarded[0]));\n";
        out += held + "    return !held;\n}\n";
        return out;
    }

private:
    // The guards asked for one term: the clause chosen, if a guard held, and
    // each guard's margin, by the clause's place.
    struct Asked {
        std::optional<int>                   chosen;
        std::map<int, std::optional<Number>> margins;
    };

    // How far a guard is from going the other way, exactly: for a comparison,
    // the distance between its sides; for 'and' and 'or', that of the operands
    // that decide. Nothing for a guard of another form.
    static std::optional<Number> Margin(Expression<Value>&        guard,
                                        EvaluationVisitor<Value>& evaluator) {
        try {
            if (auto* compare = dynamic_cast<CompareExpression<Value>*>(&guard)) {
                const Value a = compare->m_e1()->accept(evaluator);
                const Value b = compare->m_e2()->accept(evaluator);
                if (a.Size().rows * a.Size().cols != 1 || b.Size().rows * b.Size().cols != 1)
                    return std::nullopt;
                const Number difference = a(1, 1) - b(1, 1);
                return difference < Number(0) ? Number(0) - difference : difference;
            }
            if (auto* logic = dynamic_cast<LogicExpression<Value>*>(&guard)) {
                const bool both = logic->Conjunction();
                const auto near = Margin(*logic->m_e1(), evaluator);
                if (numeric_interface<Value>::truth(logic->m_e1()->accept(evaluator)) != both)
                    return near;  // the left decided
                const auto far = Margin(*logic->m_e2(), evaluator);
                if (numeric_interface<Value>::truth(logic->m_e2()->accept(evaluator)) != both)
                    return far;
                if (!near || !far) return std::nullopt;
                return *far < *near ? far : near;  // either changing changes it
            }
        } catch (const std::runtime_error&) {
        }
        return std::nullopt;
    }

    // A sequence of the instance, from its name in the struct, 'h.low.v'.
    static const Reference<Value>& Resolve(ReferenceStack<Value>& stack,
                                           const Definition& definition, const std::string& name) {
        const Scope<Value>* scope = stack.InstanceScope(definition);
        std::size_t         at    = 0;
        for (std::size_t point; scope && (point = name.find('.', at)) != std::string::npos;
             at = point + 1) {
            const auto found = scope->names.find(name.substr(at, point - at));
            scope = found == scope->names.end() ? nullptr : stack.InstanceScope(found->second);
        }
        if (scope) {
            const auto found = scope->names.find(name.substr(at));
            if (found != scope->names.end()) return *found->second;
        }
        throw std::runtime_error("the instance has no sequence " + name);
    }

    static std::string Flips(int first) {
        const std::string index = first == 0 ? "" : std::to_string(first) + " + ";
        std::string       out;
        out += "/* A guarded sequence: each clause as written and the order it is tried in,\n";
        out += " * by its place plus one, the interpreter's clause and the compiled one at\n";
        out += " * each step, 0 where no guard decided, and the interpreter's margin of each\n";
        out += " * guard it asked, by step and place, negative where none was measured. */\n";
        out += "typedef struct {\n    const char*        name;\n    int                clauses;\n";
        out += "    const char* const* written;\n    const int*         rank;\n";
        out += "    const int*         want;\n    const int*         got;\n";
        out += "    const double*      margin;\n} guarded_;\n\n";
        out += "/* The first step at which a compiled guard takes another clause, reported\n";
        out += " * before the values it makes part. Of the two clauses, the one tried first\n";
        out += " * is the one whose guard decided. */\n";
        out += "static int flips_(const guarded_* guarded, int count) {\n";
        out += "    for (int n = 0; n < " + std::to_string(steps) + "; ++n) {\n";
        out += "        for (int k = 0; k < count; ++k) {\n";
        out += "            const guarded_* g = &guarded[k];\n";
        out += "            const int got = g->got[n], want = g->want[n];\n";
        out += "            if (!got || !want || got == want) continue;\n";
        out += "            const int    compiled = g->rank[got] < g->rank[want];\n";
        out += "            const double margin =\n";
        out += "                g->margin[n * g->clauses + (compiled ? got : want) - 1];\n";
        out +=
            "            printf(\"%s: at %d the compiled step takes '%s' and the interpreter "
            "'%s'\", g->name,\n";
        out += "                   " + index + "n, g->written[got], g->written[want]);\n";
        out += "            if (margin == 0.0)\n";
        out +=
            "                printf(\"; the guard of the %s is exactly on its threshold\", "
            "compiled ? \"first\" : \"second\");\n";
        out += "            else if (margin > 0.0)\n";
        out +=
            "                printf(\"; the guard of the %s is %.2g from its threshold\", "
            "compiled ? \"first\" : \"second\", margin);\n";
        out += "            printf(\"\\n\");\n            return 0;\n        }\n    }\n";
        out += "    return 1;\n}\n\n";
        return out;
    }

    // n/a rounded down, a > 0: a term's index is negative below its first.
    static int Floor(int n, int a) { return n / a - (n % a < 0 ? 1 : 0); }

    // As C reads it back: '%.17g' writes 'inf' and 'nan', which it does not
    // (C91), and '-0', which it reads as the integer 0 (C120). NAN and
    // INFINITY are floats, whose promotion a float's program warns of (C164).
    static std::string Double(double x) {
        if (std::isnan(x)) return "(double)NAN";
        if (std::isinf(x)) return x < 0 ? "-(double)INFINITY" : "(double)INFINITY";
        char text[40];
        std::snprintf(text, sizeof text, "%.17g", x);
        const std::string written = text;
        return written.find_first_of(".e") == std::string::npos ? written + ".0" : written;
    }

    static std::string Quoted(const std::string& text) {
        std::string out = "\"";
        for (const char c : text) {
            if (c == '"' || c == '\\') out += '\\';
            out += c == '\n' ? ' ' : c;
        }
        return out + "\"";
    }

    // The interpreter's term, each cell as C writes it, or why there is none.
    struct Term {
        std::vector<std::string> cells;
        bool                     exact = true;
        std::string              error;
        std::vector<std::string> odd;  // why each cell is no finite double, or empty
        Value                    value;
        std::vector<double>      reals;          // the cells, as doubles
        bool                     spent = false;  // the interpreter ran out of steps
    };

    // How its model would state the size of an input given a matrix (C83).
    static std::string Unstated(const Model<Value>& model, const std::string& name,
                                const Term& input) {
        const auto   p    = std::find_if(model.parameters.begin(), model.parameters.end(),
                                         [&](const auto& each) { return each.name == name; });
        const Extent size = input.value.Size();
        if (p == model.parameters.end() || !p->bounds.empty()) return "";
        return ", as " + model.header.substr(0, model.header.find('(')) + " states no size for " +
               name + ": write '" + name + "_" + p->index + "[" +
               (size.slices ? "b<=" + std::to_string(size.slices) + ", " : "") +
               "j<=" + std::to_string(size.rows) +
               (size.cols > 1 || size.slices ? ", k<=" + std::to_string(size.cols) : "") + "]'";
    }

    static Term At(Interpreter<Number>& session, const std::string& name, int n) {
        const std::string term   = name + "_(" + std::to_string(n) + ")";
        const auto        result = session.Eval(term);
        Term              answer;
        if (const auto* diagnostic = std::get_if<Diagnostic>(&result)) {
            answer.error = diagnostic->message;
            answer.spent = session.Definitions().Spent();
            return answer;
        }
        const auto* value = std::get_if<Interpreter<Number>::matrix_type>(&result);
        if (!value) {
            answer.error = "it has no value";
            return answer;
        }
        answer.value = *value;
        for (std::size_t c = 0; c < value->Size().count(); ++c) {
            const Number& cell = value->data()[c];
            const auto    z    = cell.Inexact();
            answer.odd.push_back(z.imag() != 0             ? "not a real number"
                                 : std::isfinite(z.real()) ? ""
                                 : cell.exact()            ? "too large for a double"
                                                           : "not a finite number");
            answer.exact = answer.exact && cell.exact();
            answer.cells.push_back(Double(z.real()));
            answer.reals.push_back(z.real());
        }
        return answer;
    }

    // Widens each cell's estimate to a disturbed run's distance from the
    // interpreter's term: infinite where the run gives none, or one of another
    // size or not finite.
    static void Farther(const Value& term, const std::optional<Value>& run, double* about) {
        const bool same = run && run->Size() == term.Size();
        for (std::size_t c = 0; c < term.Size().count(); ++c) {
            const double d = same ? Number::abs(run->data()[c] - term.data()[c]) : HUGE_VAL;
            about[c]       = std::max(about[c], d <= HUGE_VAL ? d : HUGE_VAL);
        }
    }

    // Four values to a line, so that a term can be found by its index.
    static std::string Array(const std::string& type, const std::string& name, std::size_t size,
                             const std::vector<std::string>& values) {
        std::string out = "static " + type + " " + name + "[" + std::to_string(size) + "] = {";
        for (std::size_t k = 0; k < values.size(); ++k)
            out += (k % 4 == 0 ? "\n    " : " ") + values[k] + (k + 1 < values.size() ? "," : "");
        return out + "\n};\n";
    }
};

#endif  // INKAMATH_CHECK_HPP
