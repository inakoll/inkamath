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
        const CompileC::Compiled compiled =
            CompileC::Build(stack, module, source, unfed.model, &unfed, true);
        // Every guard asked on the way, from the first term on, by term and
        // cell: a term asked for again is remembered, and its guards are not
        // asked twice.
        std::map<std::tuple<const Reference<Value>*, int, int, int>, Asked> asked;
        stack.guards = [&](const Reference<Value>& reference, const Clause<Value>& clause, int n,
                           int row, int col, bool held, EvaluationVisitor<Value>& evaluator) {
            const int place     = static_cast<int>(&clause - reference.Clauses().data());
            Asked&    seen      = asked[{&reference, n, row, col}];
            seen.margins[place] = Margin(*clause.parameters.guard(), evaluator);
            if (held) seen.chosen = place;
        };
        struct Unhook {
            ReferenceStack<Value>& stack;
            ~Unhook() { stack.guards = nullptr; }
        } unhook{stack};
        const int         first = compiled.first;
        std::string       data, stepped, held;
        for (std::size_t k = 0; k < compiled.inputs.size(); ++k) {
            std::vector<std::string> values;
            for (int n = first; n < first + steps; ++n) {
                const Term input = At(session, instance + "." + compiled.inputs[k], n);
                if (!input.error.empty())
                    throw std::runtime_error(instance + "." + compiled.inputs[k] + "_(" +
                                             std::to_string(n) + "): " + input.error);
                if (input.cells.size() != 1)
                    throw std::runtime_error(instance + "." + compiled.inputs[k] + "_(" +
                                             std::to_string(n) + ") has " +
                                             std::to_string(input.cells.size()) +
                                             " cells, where the compiled step takes a single "
                                             "value");
                values.push_back(input.cells.front());
            }
            data += Array("const double", "in_" + std::to_string(k), steps, values);
        }
        std::optional<int> inexact;  // where the interpreter's terms stop being exact
        for (std::size_t k = 0; k < compiled.sequences.size(); ++k) {
            const auto&              sequence = compiled.sequences[k];
            const std::size_t        cells    = sequence.rows * sequence.cols;
            const std::string        id = std::to_string(k), size = std::to_string(steps * cells);
            // A term a model writes unnamed is not asked: the interpreter
            // cannot name it (C84).
            bool unnamed = false;
            try {
                (void)Resolve(stack, definition, sequence.name);
            } catch (const std::runtime_error&) {
                unnamed = true;
            }
            std::vector<std::string> want, known, why;
            for (int n = first; n < first + steps; ++n) {
                // Before its start the step has no term, and the interpreter
                // should have none either; at another rate it is not asked.
                const bool before = n < sequence.start;
                const Term term   = unnamed || (before && sequence.period > 1)
                                        ? Term{{}, true, true, "not asked"}
                                        : At(session, instance + "." + sequence.name,
                                             Floor(n - sequence.phase, sequence.period));
                if (!term.exact && (!inexact || n < *inexact)) inexact = n;
                const bool given = term.error.empty();
                const char kind  = !given         ? (before || unnamed ? '0' : '2')
                                   : !term.finite ? '4'
                                   : before       ? '3'
                                                  : '1';
                why.push_back(kind == '2' ? Quoted(term.error) : "0");
                for (std::size_t c = 0; c < cells; ++c) {
                    want.push_back(kind == '1' || kind == '3' ? term.cells[c] : "0.0");
                    known.push_back(std::string(1, kind));
                }
            }
            const bool none = std::any_of(known.begin(), known.end(),
                                          [](const std::string& each) { return each == "2"; });
            if (none) data += Array("const char* const", "why_" + id, steps, why);
            data += Array("const double", "want_" + id, steps * cells, want);
            data += Array("const unsigned char", "known_" + id, steps * cells, known);
            data += "static double got_" + id + "[" + size + "];\n";
            stepped += "        memcpy(&got_" + id + "[n * " + std::to_string(cells) + "], &m." +
                       sequence.name + "[0], sizeof(double) * " + std::to_string(cells) + ");\n";
            held += "    held &= hold_(\"" + instance + "." + sequence.name + "\", " +
                    std::to_string(sequence.cols) + ", " + std::to_string(cells) + ", " +
                    std::to_string(sequence.start) + ", got_" + id + ", want_" + id + ", known_" +
                    id + ", " + (none ? "why_" + id : std::string("0")) + ");\n";
        }
        std::string arguments;
        for (std::size_t k = 0; k < compiled.inputs.size(); ++k)
            arguments += ", in_" + std::to_string(k) + "[n]";
        std::string table;
        for (std::size_t k = 0; k < compiled.guarded.size(); ++k) {
            const std::string&       name      = compiled.guarded[k];
            const Reference<Value>&  reference = Resolve(stack, definition, name);
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
            const auto found = std::find_if(compiled.sequences.begin(), compiled.sequences.end(),
                                            [&](const auto& s) { return s.name == name; });
            // At another rate, the clause of the latest term computed, none before.
            const auto term = [&](int n) {
                return found->period > 1 && n < found->start
                           ? std::numeric_limits<int>::min()
                           : Floor(n - found->phase, found->period);
            };
            const std::size_t cols   = found->cols;
            const std::size_t places = cellwise ? found->rows * cols : 1;
            for (std::size_t c = 0; c < places; ++c) {
                const int                row = cellwise ? static_cast<int>(c / cols) + 1 : 0;
                const int                col = cellwise ? static_cast<int>(c % cols) + 1 : 0;
                const std::string        at  = id + (cellwise ? "_" + std::to_string(c) : "");
                std::vector<std::string> want, margin;
                for (int n = first; n < first + steps; ++n) {
                    const auto asking = asked.find({&reference, term(n), row, col});
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
                const std::string place =
                    cellwise ? "[" + std::to_string(row) + "," + std::to_string(col) + "]" : "";
                table += "        {\"" + instance + "." + name + place + "\", " + count +
                         ", written_" + id + ", rank_" + id + ", clause_" + at + ", taken_" + at +
                         ", margin_" + at + "},\n";
            }
        }
        const std::string against = !inexact ? "exact values"
                                             : "exact values until " + std::to_string(*inexact) +
                                                   " and inexact ones from there";

        std::string out;
        out += "/* Generated by 'inkamath --check' from " + source + ": the instance\n";
        out += " * compiled, stepped on the inputs the interpreter gives it, and each term\n";
        out += " * held to the interpreter's. Build it as the header would be built. */\n";
        out += compiled.header + "\n#include <stdio.h>\n\n";
        out += "/* A term parts from the interpreter's where they differ by more than a\n";
        out += " * billionth of one plus the interpreter's term; where the interpreter\n";
        out += " * gives none, from the sequence's start, unless it is NaN, and before it,\n";
        out += " * where the interpreter gives one. 'known' says which: 0 not asked, 1 a\n";
        out += " * term, 2 none, 3 one before the start, 4 one that is no finite double. */\n";
        out += "static int hold_(const char* name, int cols, int cells, int from,\n";
        out += "                 const double* got, const double* want,\n";
        out += "                 const unsigned char* known, const char* const* why) {\n";
        out += "    double worst = 0.0;\n";
        out += "    for (int k = 0; k < " + std::to_string(steps) + " * cells; ++k) {\n";
        out += "        const double difference = fabs(got[k] - want[k]);\n";
        out += "        const int    n = " + std::to_string(first) + " + k / cells;\n";
        out += "        if (known[k] == 0 || (known[k] == 2 && isnan(got[k]))) continue;\n";
        out += "        if (known[k] == 1 && difference <= 1e-9 * (1.0 + fabs(want[k]))) {\n";
        out += "            if (difference > worst) worst = difference;\n";
        out += "            continue;\n        }\n";
        out += "        printf(\"%s\", name);\n";
        out += "        if (cells > 1)\n";
        out += "            printf(\"[%d,%d]\", k % cells / cols + 1, k % cells % cols + 1);\n";
        out += "        if (known[k] == 3)\n";
        out += "            printf(\": none at %d, where the interpreter gives %.17g\\n\", n,\n";
        out += "                   want[k]);\n";
        out += "        else if (known[k] == 2)\n";
        out += "            printf(\": %.17g at %d, where the interpreter gives none: %s\\n\",\n";
        out += "                   got[k], n, why[k / cells]);\n";
        out += "        else if (known[k] == 4)\n";
        out += "            printf(\": %.17g at %d, where the interpreter's term is not a\"\n";
        out += "                   \" finite real number\\n\", got[k], n);\n";
        out += "        else\n";
        out += "            printf(\": %.17g at %d, where the interpreter gives %.17g\\n\",\n";
        out += "                   got[k], n, want[k]);\n";
        out += "        return 0;\n    }\n";
        out += "    printf(\"%s: within %.2g\", name, worst);\n";
        out += "    if (from > " + std::to_string(first) + ") printf(\", from %d\", from);\n";
        out += "    printf(\"\\n\");\n    return 1;\n}\n\n";
        if (!table.empty()) out += Flips(first);
        out += data + "\nint main(void) {\n";
        out += "    " + module + " m;\n    int held = 1;\n    " + module + "_init(&m);\n";
        out += "    for (int n = 0; n < " + std::to_string(steps) + "; ++n) {\n";
        out += "        " + module + "_step(&m" + arguments + ");\n" + stepped + "    }\n";
        out += "    printf(\"" + instance + ": " + std::to_string(steps) + " steps from " +
               std::to_string(first) + ", against " + against + "\\n\");\n";
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

    static std::string Double(double x) {
        char text[40];
        std::snprintf(text, sizeof text, "%.17g", x);
        return text;
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
        bool                     exact = true, finite = true;
        std::string              error;
    };

    static Term At(Interpreter<Number>& session, const std::string& name, int n) {
        const std::string term   = name + "_(" + std::to_string(n) + ")";
        const auto        result = session.Eval(term);
        Term              answer;
        if (const auto* diagnostic = std::get_if<Diagnostic>(&result)) {
            answer.error = diagnostic->message;
            return answer;
        }
        const auto* value = std::get_if<Interpreter<Number>::matrix_type>(&result);
        if (!value) {
            answer.error = "it has no value";
            return answer;
        }
        for (std::size_t i = 1; i <= value->Size().rows; ++i) {
            for (std::size_t j = 1; j <= value->Size().cols; ++j) {
                const Number& cell = (*value)(i, j);
                const auto    z    = cell.Inexact();
                answer.finite      = answer.finite && z.imag() == 0 && std::isfinite(z.real());
                answer.exact = answer.exact && cell.exact();
                answer.cells.push_back(Double(z.real()));
            }
        }
        return answer;
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
